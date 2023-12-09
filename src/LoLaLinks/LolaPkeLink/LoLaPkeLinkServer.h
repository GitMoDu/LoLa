// LoLaPkeLinkServer.h

#ifndef _LOLA_PKE_LINK_SERVER_
#define _LOLA_PKE_LINK_SERVER_

#include "..\Abstract\AbstractLoLaLinkServer.h"

#include "..\..\Crypto\LoLaCryptoPkeSession.h"

/// <summary>
/// LoLa Public-Key-Exchange Link Server.
/// Template parameters allow specifying maxlisteners.
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaPkeLinkServer : public AbstractLoLaLinkServer
{
private:
	using BaseClass = AbstractLoLaLinkServer;

public:
	static constexpr uint32_t SERVER_SLEEP_TIMEOUT_MILLIS = 10000;

private:
	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;

	enum PkeStateEnum
	{
		NoPke,
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

	PkeStateEnum PkeState = PkeStateEnum::NoPke;

	bool PkeSessionRequested = false;


public:
	LoLaPkeLinkServer(Scheduler& scheduler,
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
	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::PkeSessionRequest::HEADER:
			if (payloadSize == Unlinked::PkeSessionRequest::PAYLOAD_SIZE)
			{
				if (IsInSearchingLink())
				{
					PkeSessionRequested = true;
					Task::enable();
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Session request received."));
#endif
				}
				else if (IsInSessionCreation())
				{
					if (PkeState == PkeStateEnum::SessionCached)
					{
						Session.ResetPke();
						PkeState = PkeStateEnum::NoPke;
						StartSearching();
						PkeSessionRequested = true;
						Task::enable();
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Session request overrode state back to search."));
#endif
					}
#if defined(DEBUG_LOLA)
					else
					{
						this->Skipped(F("SessionRequest1"));
						return;
					}
#endif
				}
#if defined(DEBUG_LOLA)
				else
				{

					this->Skipped(F("SessionRequest2"));
					return;
				}
#endif
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("SessionRequest3"));
			}
#endif
			break;
		case Unlinked::PkeLinkingStartRequest::HEADER:
			if (payloadSize == Unlinked::PkeLinkingStartRequest::PAYLOAD_SIZE
				&& Session.SessionIdMatches(&payload[Unlinked::PkeLinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]))
			{
				if (IsInSearchingLink())
				{
					StartSessionCreationIfNot();
					Task::enable();
				}
				else if (IsInSessionCreation())
				{
					switch (PkeState)
					{
					case PkeStateEnum::NoPke:
					case PkeStateEnum::SessionCached:
						Task::enable();
						break;
					default:
#if defined(DEBUG_LOLA)
						this->Skipped(F("LinkingStartRequest1"));
#endif
						return;
					}
				}
				else
				{
#if defined(DEBUG_LOLA)
					this->Skipped(F("LinkingStartRequest2"));
#endif
					return;
				}

				for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::COMPRESSED_KEY_SIZE; i++)
				{
					PartnerCompressedKey[i] = payload[Unlinked::PkeLinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX + i];
				}
				PkeState = PkeStateEnum::DecompressingPartnerKey;
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("LinkingStartRequest3"));
			}
#endif
			break;
		default:
			BaseClass::OnUnlinkedPacketReceived(timestamp, payload, payloadSize, counter);
			break;
		}
	}

protected:
	virtual void ResetSessionCreation() final
	{
		PkeState = PkeStateEnum::NoPke;
		Session.ResetPke();
	}

	virtual void OnServiceSearchingLink()
	{
		if (PkeSessionRequested)
		{
			OutPacket.SetPort(Unlinked::PORT);
			OutPacket.SetHeader(Unlinked::PkeSessionAvailable::HEADER);
			for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::COMPRESSED_KEY_SIZE; i++)
			{
				OutPacket.Payload[Unlinked::PkeSessionAvailable::PAYLOAD_PUBLIC_KEY_INDEX + i] = PublicCompressedKey[i];
			}
			Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::PkeSessionAvailable::PAYLOAD_SESSION_ID_INDEX]);

			if (UnlinkedDuplexCanSend(Unlinked::PkeSessionAvailable::PAYLOAD_SIZE) &&
				PacketService.CanSendPacket())
			{
				PkeSessionRequested = false;
				if (SendPacket(OutPacket.Data, Unlinked::PkeSessionAvailable::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent PKE Session."));
#endif
				}
			}
		}
		Task::enable();
	}

	virtual void OnServiceSessionCreation() final
	{
		switch (PkeState)
		{
		case PkeStateEnum::NoPke:
			Task::enable();
			break;
		case PkeStateEnum::SessionCached:
			Task::enable();
			break;
		case PkeStateEnum::DecompressingPartnerKey:
			Session.DecompressPartnerPublicKeyFrom(PartnerCompressedKey);
			PkeState = PkeStateEnum::ValidatingSession;
			Task::enable();
			break;
		case PkeStateEnum::ValidatingSession:
			if (Session.PublicKeyCollision())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Local and Partner public keys match, link is impossible."));
#endif
				PkeState = PkeStateEnum::NoPke;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session secrets are cached, let's start linking."));
#endif
				StartSwitchToLinking();
			}
			else
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session secrets invalidated, caching..."));
#endif
				Session.ResetPke();
				PkeState = PkeStateEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case PkeStateEnum::ComputingSecretKey:
			if (Session.CalculatePke())
			{
				// PKE calculation took a lot of time, let's wait for another start request, now with cached secrets.
				PkeState = PkeStateEnum::SessionCached;
			}
			Task::enable();
			break;
		default:
			break;
		}
	}
};
#endif