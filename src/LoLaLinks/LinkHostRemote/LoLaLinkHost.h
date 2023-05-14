// LoLaLinkHost.h

#ifndef _LOLA_LINK_HOST_
#define _LOLA_LINK_HOST_

#include "..\Abstract\AbstractLoLaLink.h"


/// <summary>
/// LoLa Host Link class.
/// Template parameters allow specifying maxlisteners.
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaLinkHost : public AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;

public:
	static constexpr uint32_t HOST_SLEEP_TIMEOUT_MILLIS = 5000;

private:
	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;


	enum HostAwaitingLinkEnum
	{
		NewSessionStart,
		BroadcastingSession,
		ValidatingSession,
		ComputingSecretKey,
		SwitchingToLinking,
		Sleeping
	};

	enum HostLinkingEnum
	{
		AuthenticationRequest,
		AuthenticationReply,
		ClockSyncing,
		SwitchingToLinked
	};

protected:
	using BaseClass::LinkStage;
	using BaseClass::OutPacket;
	using BaseClass::SyncClock;
	using BaseClass::Session;
	using BaseClass::Driver;

	using BaseClass::PacketService;

	using BaseClass::ResetStageStartTime;
	using BaseClass::PreLinkResendDelayMillis;
	using BaseClass::GetSendDuration;
	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;
	//using BaseClass::SendPacketWithAck;


private:
	ServerTimedStateTransition<
		LoLaLinkDefinition::LINKING_TRANSITION_PERIOD_MICROS,
		LoLaLinkDefinition::LINKING_TRANSITION_RESEND_PERIOD_MICROS>
		StateTransition;

	uint32_t PreLinkPacketSchedule = 0;


	Timestamp InTimestamp{};
	Timestamp InEstimate{};
	TimestampError EstimateErrorReply{};

	uint8_t SubState = 0;

	bool SearchReplyPending = false;
	bool ClockReplyPending = false;

protected:
#if defined(DEBUG_LOLA)
	virtual void Owner() final
	{
		Serial.print(millis());
		Serial.print(F("\t[H] "));
	}
#endif

#if defined(HOST_DROP_LINK_TEST)
	virtual const bool CheckForClockSyncUpdate() final
	{
		if (this->GetLinkDuration() > HOST_DROP_LINK_TEST)
		{
			Serial.print(("[H] Test disconnect after "));
			Serial.print(HOST_DROP_LINK_TEST);
			Serial.println((" seconds."));
			UpdateLinkStage(LinkStageEnum::AwaitingLink);
		}

		return false;
	}
#endif

public:
	LoLaLinkHost(Scheduler& scheduler,
		ILoLaPacketDriver* driver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop,
		const uint8_t* publicKey,
		const uint8_t* privateKey,
		const uint8_t* accessPassword)
		: BaseClass(scheduler, driver, entropySource, clockSource, timerSource, duplex, hop, publicKey, privateKey, accessPassword)
		, StateTransition()
	{}
public:
#pragma region Packet Handling
protected:
	virtual void OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port, const uint8_t counter)
	{
		//#if defined(DEBUG_LOLA)
		//		this->Owner();
		//		Serial.print(F("Rx| LinkStage("));
		//		Serial.print(LinkStage);
		//		Serial.print(F(") SubState("));
		//		Serial.print(SubState);
		//		Serial.println(')');
		//#endif
		switch (port)
		{
		case Unlinked::PORT:
			if (LinkStage == LinkStageEnum::AwaitingLink)
			{
				switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
				{
				case Unlinked::SearchRequest::SUB_HEADER:
					if (payloadSize == Unlinked::SearchRequest::PAYLOAD_SIZE)
					{
						switch (SubState)
						{
						case HostAwaitingLinkEnum::Sleeping:
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Broadcast wake up!"));
#endif
							// Wake up!
							ResetStageStartTime();
							SubState = (uint8_t)HostAwaitingLinkEnum::NewSessionStart;
							PreLinkPacketSchedule = millis();
							SearchReplyPending = true;
							Task::enable();
							break;
						case HostAwaitingLinkEnum::BroadcastingSession:
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
						case HostAwaitingLinkEnum::BroadcastingSession:
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
					if (SubState == (uint8_t)HostAwaitingLinkEnum::BroadcastingSession
						&& payloadSize == Unlinked::LinkingStartRequest::PAYLOAD_SIZE
						&& Session.SessionIdMatches(&payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]))
					{
						// Slow operation (~8 ms).
						// The async alternative to inline decompressing of key would require a copy-buffer to hold the compressed key.
						Session.DecompressPartnerPublicKeyFrom(&payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX]);
						SubState = (uint8_t)HostAwaitingLinkEnum::ValidatingSession;
						Task::enable();
					}
					break;
				case Unlinked::LinkingTimedSwitchOverAck::SUB_HEADER:
					if (payloadSize == Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE
						&& SubState == (uint8_t)HostAwaitingLinkEnum::SwitchingToLinking
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
					break;
				}
			}
			break;
		case Linking::PORT:
			if (LinkStage == LinkStageEnum::Linking)
			{
				switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
				{
				case Linking::RemoteChallengeReplyRequest::SUB_HEADER:
					if (SubState == (uint8_t)HostLinkingEnum::AuthenticationRequest
						&& payloadSize == Linking::RemoteChallengeReplyRequest::PAYLOAD_SIZE
						&& Session.VerifyChallengeSignature(&payload[Linking::RemoteChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Link Access Control granted."));
#endif

						Session.SetPartnerChallenge(&payload[Linking::RemoteChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);
						SubState = (uint8_t)HostLinkingEnum::AuthenticationReply;
						Task::enable();
					}
					break;
				case Linking::ClockSyncRequest::SUB_HEADER:
					if (payloadSize == Linking::ClockSyncRequest::PAYLOAD_SIZE)
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Got ClockSyncRequest."));
#endif
						switch (SubState)
						{
						case HostLinkingEnum::ClockSyncing:
							break;
						case HostLinkingEnum::AuthenticationReply:
							SubState = (uint8_t)HostLinkingEnum::ClockSyncing;
							PreLinkPacketSchedule = millis();
							Task::enable();
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Started Clock sync"));
#endif
							break;
						case HostLinkingEnum::SwitchingToLinked:
							SubState = (uint8_t)HostLinkingEnum::ClockSyncing;
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Re-Started Clock sync"));
#endif
							break;
						default:
							break;
						}

						InEstimate.Seconds = payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX];
						InEstimate.Seconds += (uint_least16_t)payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 1] << 8;
						InEstimate.Seconds += (uint32_t)payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 2] << 16;
						InEstimate.Seconds += (uint32_t)payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 3] << 24;
						InEstimate.SubSeconds = payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX];
						InEstimate.SubSeconds += (uint_least16_t)payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 1] << 8;
						InEstimate.SubSeconds += (uint32_t)payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 2] << 16;
						InEstimate.SubSeconds += (uint32_t)payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 3] << 24;

						if (!InTimestamp.Validate())
						{
							// Invalid estimate, sub-seconds should never match one second.
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.print(F("Clock sync Invalid estimate. SubSeconds="));
							Serial.print(InTimestamp.SubSeconds);
							Serial.println(F("us. "));
#endif
							ClockReplyPending = false;
							break;
						}

						CalculateInEstimateErrorReply(startTimestamp);
						ClockReplyPending = true;
						Task::enable();
					}
					break;
				case Linking::StartLinkRequest::SUB_HEADER:
					if (payloadSize == Linking::StartLinkRequest::PAYLOAD_SIZE
						&& SubState == HostLinkingEnum::ClockSyncing)
					{

#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Got Link Start Request."));
#endif
						SubState = (uint8_t)HostLinkingEnum::SwitchingToLinked;
						StateTransition.OnStart(micros());
						Task::enableIfNot();
					}
#if defined(DEBUG_LOLA)
					else
					{
						this->Owner();
						Serial.println(F("Link Start rejected."));
					}
#endif
					break;
				case Linking::LinkTimedSwitchOverAck::SUB_HEADER:
					if (payloadSize == Linking::LinkTimedSwitchOverAck::PAYLOAD_SIZE
						&& SubState == (uint8_t)HostLinkingEnum::SwitchingToLinked)
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
					break;
				}
			}
			break;
		default:
			break;
		}
	}
#pragma endregion
#pragma region Linking
protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage) final
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::Booting:
			SetHopperFixedChannel(Session.RandomSource.GetRandomShort());
			break;
		case LinkStageEnum::AwaitingLink:
			SubState = (uint8_t)HostAwaitingLinkEnum::Sleeping;
			break;
		case LinkStageEnum::Linking:
			SubState = (uint8_t)HostLinkingEnum::AuthenticationRequest;
			ClockReplyPending = false;
			break;
		case LinkStageEnum::Linked:
			break;
		default:
			break;
		}
	}

	virtual void OnAwaitingLink(const uint32_t stateElapsed) final
	{
		if (stateElapsed > HOST_SLEEP_TIMEOUT_MILLIS)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("OnAwaitingLink timed out. Going to sleep."));
#endif

			SubState = (uint8_t)HostAwaitingLinkEnum::Sleeping;
		}

		switch (SubState)
		{
		case HostAwaitingLinkEnum::NewSessionStart:
			Session.SetRandomSessionId();
			SyncClock.ShiftSeconds(Session.RandomSource.GetRandomLong());

			SubState = (uint8_t)HostAwaitingLinkEnum::BroadcastingSession;
			PreLinkPacketSchedule = millis();
			SearchReplyPending = true;
			Task::enable();
			break;
		case HostAwaitingLinkEnum::BroadcastingSession:
			if (Driver->DriverCanTransmit() && (millis() > PreLinkPacketSchedule))
			{
				// Session broadcast packet.
				OutPacket.SetPort(Unlinked::PORT);
				if (SearchReplyPending)
				{
					OutPacket.Payload[Unlinked::SearchReply::SUB_HEADER_INDEX] = Unlinked::SearchReply::SUB_HEADER;

					if (SendPacket(this, OutPacket.Data, Unlinked::SearchReply::PAYLOAD_SIZE))
					{
						SearchReplyPending = false;
						PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent Search Reply."));
#endif
					}
				}
				else
				{
					OutPacket.Payload[Unlinked::SessionBroadcast::SUB_HEADER_INDEX] = Unlinked::SessionBroadcast::SUB_HEADER;
					Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::SessionBroadcast::PAYLOAD_SESSION_ID_INDEX]);
					Session.CompressPublicKeyTo(&OutPacket.Payload[Unlinked::SessionBroadcast::PAYLOAD_PUBLIC_KEY_INDEX]);

					if (SendPacket(this, OutPacket.Data, Unlinked::SessionBroadcast::PAYLOAD_SIZE))
					{
						PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent Broadcast Session."));
#endif
					}
				}
			}
			Task::enable();
			break;
		case HostAwaitingLinkEnum::ValidatingSession:
			if (Session.PublicKeyCollision())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Local and Partner public keys match, link is impossible."));
#endif
				SubState = (uint8_t)HostAwaitingLinkEnum::BroadcastingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session secrets are cached, let's start linking."));
#endif

				SubState = (uint8_t)HostAwaitingLinkEnum::SwitchingToLinking;
				StateTransition.OnStart(micros());
				PreLinkPacketSchedule = millis();
			}
			else
			{
				Session.ResetPke();
				SubState = (uint8_t)HostAwaitingLinkEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case HostAwaitingLinkEnum::ComputingSecretKey:
			if (Session.CalculatePke()) {
				// PKE calculation took a lot of time, let's go for another start request, now with cached secrets.
				SubState = (uint8_t)HostAwaitingLinkEnum::BroadcastingSession;
				PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);;
			}
			Task::enable();
			break;
		case HostAwaitingLinkEnum::SwitchingToLinking:
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
					SubState = (uint8_t)HostAwaitingLinkEnum::BroadcastingSession;
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
					Task::enable();
				}
			}
			else if (StateTransition.IsSendRequested(micros()) && Driver->DriverCanTransmit())
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::SUB_HEADER_INDEX] = Unlinked::LinkingTimedSwitchOver::SUB_HEADER;
				Session.CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]);
				StateTransition.CopyDurationUntilTimeOutTo(micros() + GetSendDuration(Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE), &OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				if (SendPacket(this, OutPacket.Data, Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE))
				{
					StateTransition.OnSent(micros());
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Sent Linking SwitchOver: "));
					Serial.print(StateTransition.GetDurationUntilTimeOut(micros()));
					Serial.println(F("us remaining."));
#endif
				}
			}
			else
			{
				Task::enableIfNot();
			}
			break;
		case HostAwaitingLinkEnum::Sleeping:
			Task::disable();
			break;
		default:
			break;
		}
	}

	virtual void OnLinking(const uint32_t stateElapsed) final
	{
		if (stateElapsed > LoLaLinkDefinition::LINKING_STAGE_TIMEOUT)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("Linking timed out!"));
#endif
			UpdateLinkStage(LinkStageEnum::AwaitingLink);
		}

		switch (SubState)
		{
		case HostLinkingEnum::AuthenticationRequest:
			if (Driver->DriverCanTransmit() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::HostChallengeRequest::SUB_HEADER_INDEX] = Linking::HostChallengeRequest::SUB_HEADER;
				Session.CopyLocalChallengeTo(&OutPacket.Payload[Linking::HostChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);

				if (SendPacket(this, OutPacket.Data, Linking::HostChallengeRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::HostChallengeRequest::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent HostChallengeRequest."));
#endif
				}
			}
			break;
		case HostLinkingEnum::AuthenticationReply:
			if (Driver->DriverCanTransmit() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::HostChallengeReply::SUB_HEADER_INDEX] = Linking::HostChallengeReply::SUB_HEADER;
				Session.SignPartnerChallengeTo(&OutPacket.Payload[Linking::HostChallengeReply::PAYLOAD_SIGNED_INDEX]);

				if (SendPacket(this, OutPacket.Data, Linking::HostChallengeReply::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::HostChallengeReply::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent HostChallengeReply."));
#endif
				}
			}
			break;
		case HostLinkingEnum::ClockSyncing:
			if (ClockReplyPending && Driver->DriverCanTransmit() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::ClockSyncReply::SUB_HEADER_INDEX] = Linking::ClockSyncReply::SUB_HEADER;
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 0] = EstimateErrorReply.Seconds;
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 1] = EstimateErrorReply.Seconds >> 8;
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 2] = EstimateErrorReply.Seconds >> 16;
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 3] = EstimateErrorReply.Seconds >> 24;
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 0] = EstimateErrorReply.SubSeconds;
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 1] = EstimateErrorReply.SubSeconds >> 8;
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 2] = EstimateErrorReply.SubSeconds >> 16;
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 3] = EstimateErrorReply.SubSeconds >> 24;
				if (InEstimateWithinTolerance())
				{
					OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] = UINT8_MAX;
				}
				else
				{
					OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] = 0;
				}

				if (SendPacket(this, OutPacket.Data, Linking::ClockSyncReply::PAYLOAD_SIZE))
				{
					// Only send a time reply once.
					ClockReplyPending = false;

#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Sent Clock Error:"));
					if (InEstimateWithinTolerance())
					{
						Serial.println(F("Accepted."));
					}
					else
					{
						Serial.println(F("Rejected."));
					}
#endif
				}
			}
			Task::enableIfNot();
			break;
		case HostLinkingEnum::SwitchingToLinked:
			if (StateTransition.HasTimedOut(micros()))
			{
				if (StateTransition.HasAcknowledge())
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("StateTransition success."));
#endif
					UpdateLinkStage(LinkStageEnum::Linked);
				}
				else
				{
					// No Ack before time out.
					SubState = (uint8_t)HostLinkingEnum::ClockSyncing;
					ClockReplyPending = false;
					PreLinkPacketSchedule = millis();
					Task::enable();
				}
			}
			else if (StateTransition.IsSendRequested(micros()) && Driver->DriverCanTransmit())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::LinkTimedSwitchOver::SUB_HEADER_INDEX] = Linking::LinkTimedSwitchOver::SUB_HEADER;
				StateTransition.CopyDurationUntilTimeOutTo(micros() + GetSendDuration(Linking::LinkTimedSwitchOver::PAYLOAD_SIZE), &OutPacket.Payload[Linking::LinkTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				if (SendPacket(this, OutPacket.Data, Linking::LinkTimedSwitchOver::PAYLOAD_SIZE))
				{
					StateTransition.OnSent(micros());
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Sent Link SwitchOver: "));
					Serial.print(StateTransition.GetDurationUntilTimeOut(micros()));
					Serial.println(F("us remaining."));
#endif
				}
			}
			else
			{
				Task::enableIfNot();
			}
			break;
		default:
			break;
		}
	}


#pragma endregion
private:
	void CalculateInEstimateErrorReply(const uint32_t receiveTimestamp)
	{
		const int32_t elapsedSinceReceived = micros() - receiveTimestamp;

		SyncClock.CalculateError(EstimateErrorReply, InEstimate, -elapsedSinceReceived);

#if defined(DEBUG_LOLA)
		this->Owner();
		Serial.print(F("\tError: "));
		Serial.println(EstimateErrorReply.ErrorMicros());
#endif
	}

	const bool InEstimateWithinTolerance()
	{
		const int32_t errorMicros = EstimateErrorReply.ErrorMicros();

		if (errorMicros >= 0)
		{
			return errorMicros < LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
		}
		else
		{
			return (-errorMicros) < (int32_t)LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
		}
	}
};
#endif