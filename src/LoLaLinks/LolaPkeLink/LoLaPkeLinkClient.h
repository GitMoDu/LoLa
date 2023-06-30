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
		ValidatingSession,
		ComputingSecretKey,
		SessionCached
	};

protected:
	using BaseClass::ExpandedKey;
	using BaseClass::OutPacket;
	using BaseClass::PacketService;

	using BaseClass::GetElapsedMicrosSinceLastUnlinkedSent;
	using BaseClass::IsInSessionCreation;

	using BaseClass::SendPacket;

private:
	LoLaCryptoPkeSession Session;

	PkeStateEnum PkeState = PkeStateEnum::RequestingSession;

	bool SessionRequestReplyPending = false;

public:
	LoLaPkeLinkClient(Scheduler& scheduler,
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
		case Unlinked::SessionBroadcast::SUB_HEADER:
			if (payloadSize == Unlinked::SessionBroadcast::PAYLOAD_SIZE
				&& IsInSessionCreation())
			{
				switch (PkeState)
				{
				case PkeStateEnum::RequestingSession:
					PkeState = PkeStateEnum::ValidatingSession;
					Session.SetSessionId(&payload[Unlinked::SessionBroadcast::PAYLOAD_SESSION_ID_INDEX]);

					// Slow operation (~8 ms)... but this call is already async and don't need to store the partners compressed public key.
					Session.DecompressPartnerPublicKeyFrom(&payload[Unlinked::SessionBroadcast::PAYLOAD_PUBLIC_KEY_INDEX]);
					Task::enable();

#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Found a server! (session broadcast)"));
#endif
					break;
				default:
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Rejected a server! (session broadcast). PkeState: "));
					Serial.println(PkeState);
#endif
					break;
				}
			}
			break;
		default:
			BaseClass::OnUnlinkedPacketReceived(startTimestamp, payload, payloadSize, counter);
			break;
		}
	}

protected:
	virtual void ResetSessionCreation() final
	{
		PkeState = PkeStateEnum::RequestingSession;
	}

	virtual void OnServiceSessionCreation() final
	{
		switch (PkeState)
		{
		case PkeStateEnum::RequestingSession:
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::SessionRequest::SUB_HEADER_INDEX] = Unlinked::SessionRequest::SUB_HEADER;
				if (SendPacket(OutPacket.Data, Unlinked::SessionRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Session Start Request."));
#endif
				}
				else
				{
					Task::enable();
				}
			}
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
				Serial.println(F("Session token is cached, trying LinkingStartRequest."));
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
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::LinkingStartRequest::SUB_HEADER_INDEX] = Unlinked::LinkingStartRequest::SUB_HEADER;
				Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]);
				Session.CompressPublicKeyTo(&OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX]);

				if (SendPacket(OutPacket.Data, Unlinked::LinkingStartRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Linking Start Request."));
#endif
				}
			}
			Task::enable();
			break;
		}
	}
};
#endif