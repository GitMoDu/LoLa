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
class LoLaPkeLinkClient : public AbstractLoLaLinkClient<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLinkClient<MaxPacketReceiveListeners, MaxLinkListeners>;

private:
	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;

	enum PkeStateEnum
	{
		RequestingSession,
		DecompressingPartnerKey,
		ValidatingSession,
		ComputingSecretKey,
		SessionCached
	};

protected:
	using BaseClass::ExpandedKey;
	using BaseClass::OutPacket;
	using BaseClass::PacketService;

	using BaseClass::IsInSessionCreation;
	using BaseClass::OnLinkSyncReceived;
	using BaseClass::UnlinkedCanSendPacket;
	using BaseClass::UnlinkedPacketThrottle;
	using BaseClass::ResetUnlinkedPacketThrottle;

	using BaseClass::SendPacket;

private:
	uint8_t PublicCompressedKey[LoLaCryptoDefinition::COMPRESSED_KEY_SIZE]{};
	uint8_t PartnerCompressedKey[LoLaCryptoDefinition::COMPRESSED_KEY_SIZE]{};

private:
	LoLaCryptoPkeSession Session;

	PkeStateEnum PkeState = PkeStateEnum::RequestingSession;

	bool SessionRequestReplyPending = false;

public:
	LoLaPkeLinkClient(Scheduler& scheduler,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop,
		const uint8_t* publicKey,
		const uint8_t* privateKey,
		const uint8_t* accessPassword)
		: BaseClass(scheduler, &Session, transceiver, cycles, entropy, duplex, hop)
		, Session(&ExpandedKey, accessPassword, publicKey, privateKey)
	{}

	virtual const bool Setup()
	{
		if (Session.Setup())
		{
			Session.CompressPublicKeyTo(PublicCompressedKey);

			return BaseClass::Setup();
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
		case Unlinked::SessionAvailable::HEADER:
			if (payloadSize == Unlinked::SessionAvailable::PAYLOAD_SIZE
				&& IsInSessionCreation())
			{
				switch (PkeState)
				{
				case PkeStateEnum::RequestingSession:;
					PkeState = PkeStateEnum::DecompressingPartnerKey;
					Session.SetSessionId(&payload[Unlinked::SessionAvailable::PAYLOAD_SESSION_ID_INDEX]);
					ResetUnlinkedPacketThrottle();

					for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::COMPRESSED_KEY_SIZE; i++)
					{
						PartnerCompressedKey[i] = payload[Unlinked::SessionAvailable::PAYLOAD_PUBLIC_KEY_INDEX + i];
					}
					Task::enable();
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Found a PKE Session."));
#endif
					OnLinkSyncReceived(timestamp);
					break;
				default:
#if defined(DEBUG_LOLA)
					this->Skipped(F("SessionAvailable"));
#endif
					break;
				}
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("SessionAvailable"));
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
		PkeState = PkeStateEnum::RequestingSession;
		ResetUnlinkedPacketThrottle();
		Task::enable();
	}

	virtual void OnServiceSessionCreation() final
	{
		switch (PkeState)
		{
		case PkeStateEnum::RequestingSession:
			// Wait longer because reply is big.
			if (UnlinkedPacketThrottle()
				&& UnlinkedCanSendPacket(Unlinked::SessionRequest::PAYLOAD_SIZE))
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.SetHeader(Unlinked::SessionRequest::HEADER);
				if (SendPacket(OutPacket.Data, Unlinked::SessionRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Session Start Request."));
#endif
				}
			}
			Task::enable();
			break;
		case PkeStateEnum::DecompressingPartnerKey:
			PkeState = PkeStateEnum::ValidatingSession;
			Session.DecompressPartnerPublicKeyFrom(PartnerCompressedKey);
			Task::enable();
			break;
		case PkeStateEnum::ValidatingSession:
			if (Session.PublicKeyCollision())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Local and Partner public keys match, link is impossible."));
#endif
				PkeState = PkeStateEnum::RequestingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session token is cached, Linking."));
#endif
				PkeState = PkeStateEnum::SessionCached;
			}
			else
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session token needs refreshing."));
#endif
				Session.ResetPke();
				PkeState = PkeStateEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case PkeStateEnum::ComputingSecretKey:
			if (Session.CalculatePke())
			{
				// PKE calculation took a lot of time, let's reset to start now with a cached key-pair.
				PkeState = PkeStateEnum::ValidatingSession;
			}
			Task::enable();
			break;
		case PkeStateEnum::SessionCached:
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.SetHeader(Unlinked::LinkingStartRequest::HEADER);
				for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::COMPRESSED_KEY_SIZE; i++)
				{
					OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX + i] = PublicCompressedKey[i];
				}
				Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]);

				if (UnlinkedCanSendPacket(Unlinked::LinkingStartRequest::PAYLOAD_SIZE))
				{
					if (SendPacket(OutPacket.Data, Unlinked::LinkingStartRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent Linking Start Request."));
#endif
					}
				}
			}
			Task::enable();
			break;
		}
	}
};
#endif