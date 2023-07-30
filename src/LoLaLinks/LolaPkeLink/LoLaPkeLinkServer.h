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
class LoLaPkeLinkServer : public AbstractLoLaLinkServer<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLinkServer<MaxPacketReceiveListeners, MaxLinkListeners>;

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

protected:
	using BaseClass::ExpandedKey;
	using BaseClass::OutPacket;
	using BaseClass::PacketService;

	using BaseClass::StartSwitchToLinking;
	using BaseClass::IsInSessionCreation;
	using BaseClass::StartSearching;
	using BaseClass::IsInSearchingLink;
	using BaseClass::StartSessionCreationIfNot;
	using BaseClass::GetElapsedMicrosSinceLastUnlinkedSent;
	using BaseClass::CanSendLinkingPacket;

	using BaseClass::SendPacket;

private:
	uint8_t PublicCompressedKey[LoLaCryptoDefinition::COMPRESSED_KEY_SIZE]{};
	uint8_t PartnerCompressedKey[LoLaCryptoDefinition::COMPRESSED_KEY_SIZE]{};

private:
	LoLaCryptoPkeSession Session;

	PkeStateEnum PkeState = PkeStateEnum::NoPke;

	bool PkeSessionRequested = false;


public:
	LoLaPkeLinkServer(Scheduler& scheduler,
		ILoLaTransceiver* transceiver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop,
		const uint8_t* publicKey,
		const uint8_t* privateKey,
		const uint8_t* accessPassword)
		: BaseClass(scheduler, &Session, transceiver, entropySource, clockSource, timerSource, duplex, hop)
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
	virtual void OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) final
	{
		switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
		{
		case Unlinked::SessionRequest::SUB_HEADER:
			if (payloadSize == Unlinked::SessionRequest::PAYLOAD_SIZE)
			{
				if (IsInSearchingLink())
				{
					PkeSessionRequested = true;
				}
				else if (IsInSessionCreation())
				{
					PkeSessionRequested = true;
					if (PkeState == PkeStateEnum::SessionCached)
					{
						StartSearching();
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Session request overrode state back to search."));
#endif
					}
				}
#if defined(DEBUG_LOLA)
				else
				{
					this->Skipped(F("SessionRequest1"));
					return;
				}
#endif
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.print(F("Session request received. PKE State: "));
				Serial.println(PkeState);
#endif
				Task::enable();
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("SessionRequest2"));
			}
#endif
			break;
		case Unlinked::LinkingStartRequest::SUB_HEADER:
			if (payloadSize == Unlinked::LinkingStartRequest::PAYLOAD_SIZE
				&& Session.SessionIdMatches(&payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]))
			{
				switch (PkeState)
				{
				case PkeStateEnum::NoPke:
				case PkeStateEnum::SessionCached:
					break;
				default:
#if defined(DEBUG_LOLA)
					this->Skipped(F("LinkingStartRequest1"));
#endif
					return;
				}

				StartSessionCreationIfNot();

				for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::COMPRESSED_KEY_SIZE; i++)
				{
					PartnerCompressedKey[i] = payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX + i];
				}
				PkeState = PkeStateEnum::DecompressingPartnerKey;
				Task::enable();
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("LinkingStartRequest2"));
			}
#endif
			break;
		default:
			BaseClass::OnUnlinkedPacketReceived(startTimestamp, payload, payloadSize, counter);
			break;
		}
	}

protected:
	virtual void ResetSessionCreation() final
	{
		PkeState = PkeStateEnum::NoPke;
	}

	virtual void OnServiceSearchingLink()
	{
		if (PkeSessionRequested)
		{
			OutPacket.SetPort(Unlinked::PORT);
			OutPacket.Payload[Unlinked::SessionAvailable::SUB_HEADER_INDEX] = Unlinked::SessionAvailable::SUB_HEADER;
			for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::COMPRESSED_KEY_SIZE; i++)
			{
				OutPacket.Payload[Unlinked::SessionAvailable::PAYLOAD_PUBLIC_KEY_INDEX + i] = PublicCompressedKey[i];
			}
			Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::SessionAvailable::PAYLOAD_SESSION_ID_INDEX]);

			if (CanSendLinkingPacket(Unlinked::SessionAvailable::PAYLOAD_SIZE))
			{
				if (SendPacket(OutPacket.Data, Unlinked::SessionAvailable::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent PKE Session."));
#endif
				}
				PkeSessionRequested = false;
			}
		}
		Task::enable();
	}

	virtual void OnServiceSessionCreation() final
	{
		if (PkeSessionRequested)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("Session request caught off timeline."));
#endif
			PkeSessionRequested = false;
		}

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