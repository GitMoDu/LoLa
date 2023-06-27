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

	enum ServerAwaitingLinkEnum
	{
		BroadcastingSession,
		ValidatingSession,
		ComputingSecretKey,
	};

protected:
	using BaseClass::ExpandedKey;
	using BaseClass::OutPacket;
	using BaseClass::PacketService;

	using BaseClass::StartSwitchToLinking;
	using BaseClass::IsInSessionCreation;
	using BaseClass::IsInSearchingLink;
	using BaseClass::StartSessionCreationIfNot;
	using BaseClass::GetElapsedMicrosSinceLastUnlinkedSent;

	using BaseClass::SendPacket;

private:
	LoLaCryptoPkeSession Session;

	ServerAwaitingLinkEnum PkeState = ServerAwaitingLinkEnum::BroadcastingSession;

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
		return Session.Setup() && BaseClass::Setup();
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
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Session request received."));
#endif
					Task::enable();
					PkeState = ServerAwaitingLinkEnum::BroadcastingSession;
					StartSessionCreationIfNot();
				}
				else
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Session request ignored."));
#endif
				}
			}
			break;
		case Unlinked::LinkingStartRequest::SUB_HEADER:
			if (IsInSessionCreation()
				&& PkeState == ServerAwaitingLinkEnum::BroadcastingSession
				&& payloadSize == Unlinked::LinkingStartRequest::PAYLOAD_SIZE
				&& Session.SessionIdMatches(&payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]))
			{
				// Slow operation (~8 ms).
				// The async alternative to inline decompressing of key would require a copy-buffer to hold the compressed key.
				Session.DecompressPartnerPublicKeyFrom(&payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX]);
				PkeState = ServerAwaitingLinkEnum::ValidatingSession;
				Task::enable();
			}
			break;
		default:
			BaseClass::OnUnlinkedPacketReceived(startTimestamp, payload, payloadSize, counter);
			break;
		}
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage) final
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::Booting:
			break;
		case LinkStageEnum::AwaitingLink:
			PkeState = ServerAwaitingLinkEnum::BroadcastingSession;
			break;
		case LinkStageEnum::Linking:
			break;
		case LinkStageEnum::Linked:
			break;
		default:
			break;
		}
	}

	virtual void OnServiceSessionCreation() final
	{
		switch (PkeState)
		{
		case ServerAwaitingLinkEnum::BroadcastingSession:
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::REPLY_BASE_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::SessionBroadcast::SUB_HEADER_INDEX] = Unlinked::SessionBroadcast::SUB_HEADER;
				Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::SessionBroadcast::PAYLOAD_SESSION_ID_INDEX]);
				Session.CompressPublicKeyTo(&OutPacket.Payload[Unlinked::SessionBroadcast::PAYLOAD_PUBLIC_KEY_INDEX]);

				if (SendPacket(OutPacket.Data, Unlinked::SessionBroadcast::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent PKE Session."));
#endif
				}
				else
				{
					Task::enable();
				}
			}
			else
			{
				Task::delay(1);
			}
			break;
		case ServerAwaitingLinkEnum::ValidatingSession:
			if (Session.PublicKeyCollision())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Local and Partner public keys match, link is impossible."));
#endif
				PkeState = ServerAwaitingLinkEnum::BroadcastingSession;
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
				Session.ResetPke();
				PkeState = ServerAwaitingLinkEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case ServerAwaitingLinkEnum::ComputingSecretKey:
			if (Session.CalculatePke())
			{
				// PKE calculation took a lot of time, let's go for another start request, now with cached secrets.
				PkeState = ServerAwaitingLinkEnum::BroadcastingSession;
			}
			Task::enable();
			break;
		default:
			break;
		}
	}
};
#endif