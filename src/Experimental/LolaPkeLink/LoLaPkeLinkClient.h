// LoLaPkeLinkClient.h

#ifndef _LOLA_PKE_LINK_CLIENT_
#define _LOLA_PKE_LINK_CLIENT_

#include "..\Abstract\AbstractLoLaLinkClient.h"

#include "..\..\Crypto\LoLaCryptoPkeSession.h"

/// <summary>
/// LoLa Public-Key-Exchange Link Client.
/// Template parameters allow specifying maxlisteners.
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaPkeLinkClient : public AbstractLoLaLinkClient
{
private:
	using BaseClass = AbstractLoLaLinkClient;

	enum class PkeStateEnum : uint8_t
	{
		RequestingSession,
		DecompressingPartnerKey,
		ValidatingSession,
		ComputingSecretKey,
		SessionCached
	};

private:
	uint8_t PublicCompressedKey[LoLaCryptoDefinition::COMPRESSED_KEY_SIZE]{};
	uint8_t PartnerCompressedKey[LoLaCryptoDefinition::COMPRESSED_KEY_SIZE]{};

private:
	LinkRegistry<MaxPacketReceiveListeners, MaxLinkListeners> RegistryInstance{};

	LoLaCryptoPkeSession Session;

	PkeStateEnum PkeState = PkeStateEnum::RequestingSession;

public:
	LoLaPkeLinkClient(TS::Scheduler& scheduler,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop,
		const uint8_t* accessPassword,
		const uint8_t* publicKey,
		const uint8_t* privateKey)
		: BaseClass(scheduler, &RegistryInstance, &Session, transceiver, cycles, entropy, duplex, hop)
		, Session(accessPassword, publicKey, privateKey)
	{}

	const bool Setup()
	{
		if (Session.Setup())
		{
			Session.CompressPublicKeyTo(PublicCompressedKey);

			return RegistryInstance.Setup() && BaseClass::Setup();
		}
#if defined(DEBUG_LOLA)
		else
		{
			this->Owner();
			Serial.println(F("PKE Session setup failed."));
		}
#endif
		return false;
	}

protected:
	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint16_t rollingCounter, const uint8_t payloadSize) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::PkeSessionAvailable::HEADER:
			if (payloadSize == Unlinked::PkeSessionAvailable::PAYLOAD_SIZE
				&& IsInSessionCreation())
			{
				switch (PkeState)
				{
				case PkeStateEnum::RequestingSession:;
					PkeState = PkeStateEnum::DecompressingPartnerKey;
					Session.SetSessionId(&payload[Unlinked::PkeSessionAvailable::PAYLOAD_SESSION_ID_INDEX]);
					OnLinkSyncReceived(timestamp);
					ResetUnlinkedPacketThrottle();

					for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::COMPRESSED_KEY_SIZE; i++)
					{
						PartnerCompressedKey[i] = payload[Unlinked::PkeSessionAvailable::PAYLOAD_PUBLIC_KEY_INDEX + i];
					}
					TS::Task::enable();
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Found a PKE Session."));
#endif
					break;
				default:
#if defined(DEBUG_LOLA_LINK)
					this->Skipped(F("SessionAvailable"));
#endif
					break;
				}
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("SessionAvailable")); }
#endif
			break;
		default:
			BaseClass::OnUnlinkedPacketReceived(timestamp, payload, rollingCounter, payloadSize);
			break;
		}
	}

protected:
	virtual void ResetSessionCreation() final
	{
		PkeState = PkeStateEnum::RequestingSession;
		ResetUnlinkedPacketThrottle();
		TS::Task::enable();
	}

	virtual void OnServiceSessionCreation() final
	{
		switch (PkeState)
		{
		case PkeStateEnum::RequestingSession:
			// Wait longer because reply is big.
			if (UnlinkedPacketThrottle())
			{
				LOLA_RTOS_PAUSE();
				if (PacketService.CanSendPacket())
				{
					OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
					OutPacket.SetHeader(Unlinked::PkeSessionRequest::HEADER);
					if (SendPacket(OutPacket.Data, Unlinked::PkeSessionRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent Session Start Request."));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
			TS::Task::enable();
			break;
		case PkeStateEnum::DecompressingPartnerKey:
			PkeState = PkeStateEnum::ValidatingSession;
			Session.DecompressPartnerPublicKeyFrom(PartnerCompressedKey);
			TS::Task::enable();
			break;
		case PkeStateEnum::ValidatingSession:
			if (Session.PublicKeyCollision())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Local and Partner public keys match, link is impossible."));
#endif
				PkeState = PkeStateEnum::RequestingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Session token is cached, Linking."));
#endif
				PkeState = PkeStateEnum::SessionCached;
			}
			else
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Session token needs refreshing."));
#endif
				Session.ResetPke();
				PkeState = PkeStateEnum::ComputingSecretKey;
			}
			TS::Task::enable();
			break;
		case PkeStateEnum::ComputingSecretKey:
			if (Session.CalculatePke())
			{
				// PKE calculation took a lot of time, let's reset to start now with a cached key-pair.
				PkeState = PkeStateEnum::ValidatingSession;
			}
			TS::Task::enable();
			break;
		case PkeStateEnum::SessionCached:
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Unlinked::PkeLinkingStartRequest::HEADER);
				for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::COMPRESSED_KEY_SIZE; i++)
				{
					OutPacket.Payload[Unlinked::PkeLinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX + i] = PublicCompressedKey[i];
				}
				Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::PkeLinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]);

				LOLA_RTOS_PAUSE();
				if (UnlinkedDuplexCanSend(Unlinked::PkeLinkingStartRequest::PAYLOAD_SIZE) &&
					PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Unlinked::PkeLinkingStartRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent Linking Start Request."));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
			TS::Task::enable();
			break;
		}
	}
};
#endif