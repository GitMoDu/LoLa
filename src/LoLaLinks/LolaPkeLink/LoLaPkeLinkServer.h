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
		NewSessionStart,
		BroadcastingSession,
		ValidatingSession,
		ComputingSecretKey,
		SwitchingToLinking,
		Sleeping
	};

protected:
	using BaseClass::ExpandedKey;
	using BaseClass::LinkStage;
	using BaseClass::OutPacket;
	using BaseClass::SyncClock;
	using BaseClass::RandomSource;
	using BaseClass::Transceiver;

	using BaseClass::StateTransition;
	using BaseClass::PreLinkPacketSchedule;

	using BaseClass::ResetStageStartTime;
	using BaseClass::PreLinkResendDelayMillis;
	using BaseClass::GetSendDuration;
	using BaseClass::GetStageElapsedMillis;
	using BaseClass::SendPacket;

private:
	LoLaCryptoPkeSession Session;

	ServerAwaitingLinkEnum SubState = ServerAwaitingLinkEnum::Sleeping;

	bool SearchReplyPending = false;

protected:
#if defined(DEBUG_LOLA)
	virtual void Owner() final
	{
		Serial.print(millis());
		Serial.print(F("\t[S] "));
	}
#endif

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
		case Unlinked::SearchRequest::SUB_HEADER:
			if (payloadSize == Unlinked::SearchRequest::PAYLOAD_SIZE)
			{
				switch (SubState)
				{
				case ServerAwaitingLinkEnum::Sleeping:
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Broadcast wake up!"));
#endif
					// Wake up!
					ResetStageStartTime();
					SubState = ServerAwaitingLinkEnum::NewSessionStart;
					PreLinkPacketSchedule = millis();
					SearchReplyPending = true;
					Task::enable();
					break;
				case ServerAwaitingLinkEnum::BroadcastingSession:
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
					Task::enable();
					SearchReplyPending = true;
					break;
				default:
					break;
				}
			}
			break;
		case Unlinked::SessionRequest::SUB_HEADER:
			if (payloadSize == Unlinked::SessionRequest::PAYLOAD_SIZE)
			{
				switch (SubState)
				{
				case ServerAwaitingLinkEnum::BroadcastingSession:
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Session start request received."));
#endif
					SearchReplyPending = false;
					Task::enable();
					break;
				default:
					break;
				}
			}
			break;
		case Unlinked::LinkingStartRequest::SUB_HEADER:
			if (SubState == ServerAwaitingLinkEnum::BroadcastingSession
				&& payloadSize == Unlinked::LinkingStartRequest::PAYLOAD_SIZE
				&& Session.SessionIdMatches(&payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]))
			{
				// Slow operation (~8 ms).
				// The async alternative to inline decompressing of key would require a copy-buffer to hold the compressed key.
				Session.DecompressPartnerPublicKeyFrom(&payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX]);
				SubState = ServerAwaitingLinkEnum::ValidatingSession;
				Task::enable();
			}
			break;
		case Unlinked::LinkingTimedSwitchOverAck::SUB_HEADER:
			if (payloadSize == Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE
				&& SubState == ServerAwaitingLinkEnum::SwitchingToLinking
				&& Session.LinkingTokenMatches(&payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SESSION_TOKEN_INDEX]))
			{
				StateTransition.OnReceived();
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.print(F("StateTransition Server got Ack: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(micros()));
				Serial.println(F("us remaining."));
#endif
				Task::enable();
			}
			break;
		default:
			OnUnlinkedPacketReceived(startTimestamp, payload, payloadSize, counter);
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
			SearchReplyPending = false;
			break;
		case LinkStageEnum::AwaitingLink:
			SubState = ServerAwaitingLinkEnum::Sleeping;
			break;
		case LinkStageEnum::Linking:
			break;
		case LinkStageEnum::Linked:
			break;
		default:
			break;
		}
	}

	virtual void OnServiceAwaitingLink() final
	{
		if (GetStageElapsedMillis() > SERVER_SLEEP_TIMEOUT_MILLIS)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("OnAwaitingLink timed out. Going to sleep."));
#endif

			SubState = ServerAwaitingLinkEnum::Sleeping;
		}

		switch (SubState)
		{
		case ServerAwaitingLinkEnum::NewSessionStart:
			Session.SetRandomSessionId(&RandomSource);
			SyncClock.ShiftSeconds(RandomSource.GetRandomLong());

			SubState = ServerAwaitingLinkEnum::BroadcastingSession;
			PreLinkPacketSchedule = millis();
			SearchReplyPending = true;
			Task::enable();
			break;
		case ServerAwaitingLinkEnum::BroadcastingSession:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				// Session broadcast packet.
				OutPacket.SetPort(Unlinked::PORT);
				if (SearchReplyPending)
				{
					OutPacket.Payload[Unlinked::SearchReply::SUB_HEADER_INDEX] = Unlinked::SearchReply::SUB_HEADER;

#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sending Search Reply."));
#endif
					if (SendPacket(OutPacket.Data, Unlinked::SearchReply::PAYLOAD_SIZE))
					{
						SearchReplyPending = false;
						PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
					}
				}
				else
				{
					OutPacket.Payload[Unlinked::SessionBroadcast::SUB_HEADER_INDEX] = Unlinked::SessionBroadcast::SUB_HEADER;
					Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::SessionBroadcast::PAYLOAD_SESSION_ID_INDEX]);
					Session.CompressPublicKeyTo(&OutPacket.Payload[Unlinked::SessionBroadcast::PAYLOAD_PUBLIC_KEY_INDEX]);

#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sending Broadcast Session."));
#endif
					if (SendPacket(OutPacket.Data, Unlinked::SessionBroadcast::PAYLOAD_SIZE))
					{
						PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
					}
				}
			}
			Task::enable();
			break;
		case ServerAwaitingLinkEnum::ValidatingSession:
			if (Session.PublicKeyCollision())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Local and Partner public keys match, link is impossible."));
#endif
				SubState = ServerAwaitingLinkEnum::BroadcastingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session secrets are cached, let's start linking."));
#endif

				SubState = ServerAwaitingLinkEnum::SwitchingToLinking;
				StateTransition.OnStart(micros());
				PreLinkPacketSchedule = millis();
			}
			else
			{
				Session.ResetPke();
				SubState = ServerAwaitingLinkEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case ServerAwaitingLinkEnum::ComputingSecretKey:
			if (Session.CalculatePke()) {
				// PKE calculation took a lot of time, let's go for another start request, now with cached secrets.
				SubState = ServerAwaitingLinkEnum::BroadcastingSession;
				PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);;
			}
			Task::enable();
			break;
		case ServerAwaitingLinkEnum::SwitchingToLinking:
			if (StateTransition.HasTimedOut(micros()))
			{
				if (StateTransition.HasAcknowledge())
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("StateTransition success."));
#endif
					UpdateLinkStage(LinkStageEnum::Linking);
				}
				else
				{
					// No Ack before time out.
					SubState = ServerAwaitingLinkEnum::BroadcastingSession;
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
					Task::enable();
				}
			}
			else if (StateTransition.IsSendRequested(micros()) && Transceiver->TxAvailable())
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::SUB_HEADER_INDEX] = Unlinked::LinkingTimedSwitchOver::SUB_HEADER;
				Session.CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]);
				StateTransition.CopyDurationUntilTimeOutTo(micros() + GetSendDuration(Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE), &OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_TIME_INDEX]);

				if (SendPacket(OutPacket.Data, Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE))
				{
					StateTransition.OnSent(micros());
				}
			}
			else
			{
				Task::enableIfNot();
			}
			break;
		case ServerAwaitingLinkEnum::Sleeping:
			Task::disable();
			break;
		default:
			break;
		}
	}
};
#endif