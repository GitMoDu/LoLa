// LoLaPkeLinkServer.h

#ifndef _LOLA_PKE_LINK_SERVER_
#define _LOLA_PKE_LINK_SERVER_

#include "..\Abstract\AbstractPublicKeyLoLaLink.h"

/// <summary>
/// LoLa Public-Key-Exchange Link Server.
/// Template parameters allow specifying maxlisteners.
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaPkeLinkServer : public AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;

public:
	static constexpr uint32_t SERVER_SLEEP_TIMEOUT_MILLIS = 5000;

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

	enum ServerLinkingEnum
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
	using BaseClass::RandomSource;
	using BaseClass::Transceiver;

	using BaseClass::PacketService;

	using BaseClass::ResetStageStartTime;
	using BaseClass::PreLinkResendDelayMillis;
	using BaseClass::GetSendDuration;
	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;


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
		Serial.print(F("\t[S] "));
	}
#endif

#if defined(SERVER_DROP_LINK_TEST)
	virtual const bool CheckForClockSyncUpdate() final
	{
		if (this->GetLinkDuration() > SERVER_DROP_LINK_TEST)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.print(("Test disconnect after "));
			Serial.print(SERVER_DROP_LINK_TEST);
			Serial.println((" seconds."));
#endif
			UpdateLinkStage(LinkStageEnum::AwaitingLink);
		}

		return false;
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
		: BaseClass(scheduler, transceiver, entropySource, clockSource, timerSource, duplex, hop, publicKey, privateKey, accessPassword)
		, StateTransition()
	{}

public:
#pragma region Packet Handling
protected:
	virtual void OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port, const uint8_t counter)
	{
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
						case ServerAwaitingLinkEnum::Sleeping:
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Broadcast wake up!"));
#endif
							// Wake up!
							ResetStageStartTime();
							SubState = (uint8_t)ServerAwaitingLinkEnum::NewSessionStart;
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
					if (SubState == (uint8_t)ServerAwaitingLinkEnum::BroadcastingSession
						&& payloadSize == Unlinked::LinkingStartRequest::PAYLOAD_SIZE
						&& Session.SessionIdMatches(&payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]))
					{
						// Slow operation (~8 ms).
						// The async alternative to inline decompressing of key would require a copy-buffer to hold the compressed key.
						Session.DecompressPartnerPublicKeyFrom(&payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX]);
						SubState = (uint8_t)ServerAwaitingLinkEnum::ValidatingSession;
						Task::enable();
					}
					break;
				case Unlinked::LinkingTimedSwitchOverAck::SUB_HEADER:
					if (payloadSize == Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE
						&& SubState == (uint8_t)ServerAwaitingLinkEnum::SwitchingToLinking
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
				case Linking::ClientChallengeReplyRequest::SUB_HEADER:
					if (SubState == (uint8_t)ServerLinkingEnum::AuthenticationRequest
						&& payloadSize == Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE
						&& Session.VerifyChallengeSignature(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Link Access Control granted."));
#endif

						Session.SetPartnerChallenge(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);
						SubState = (uint8_t)ServerLinkingEnum::AuthenticationReply;
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
						case ServerLinkingEnum::ClockSyncing:
							break;
						case ServerLinkingEnum::AuthenticationReply:
							SubState = (uint8_t)ServerLinkingEnum::ClockSyncing;
							PreLinkPacketSchedule = millis();
							Task::enable();
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Started Clock sync"));
#endif
							break;
						case ServerLinkingEnum::SwitchingToLinked:
							SubState = (uint8_t)ServerLinkingEnum::ClockSyncing;
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
						&& SubState == ServerLinkingEnum::ClockSyncing)
					{

#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Got Link Start Request."));
#endif
						SubState = (uint8_t)ServerLinkingEnum::SwitchingToLinked;
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
						&& SubState == (uint8_t)ServerLinkingEnum::SwitchingToLinked)
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
			SetHopperFixedChannel(RandomSource.GetRandomShort());
			break;
		case LinkStageEnum::AwaitingLink:
			SubState = (uint8_t)ServerAwaitingLinkEnum::Sleeping;
			break;
		case LinkStageEnum::Linking:
			SubState = (uint8_t)ServerLinkingEnum::AuthenticationRequest;
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
		if (stateElapsed > SERVER_SLEEP_TIMEOUT_MILLIS)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("OnAwaitingLink timed out. Going to sleep."));
#endif

			SubState = (uint8_t)ServerAwaitingLinkEnum::Sleeping;
		}

		switch (SubState)
		{
		case ServerAwaitingLinkEnum::NewSessionStart:
			Session.SetRandomSessionId(&RandomSource);
			SyncClock.ShiftSeconds(RandomSource.GetRandomLong());

			SubState = (uint8_t)ServerAwaitingLinkEnum::BroadcastingSession;
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
				SubState = (uint8_t)ServerAwaitingLinkEnum::BroadcastingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session secrets are cached, let's start linking."));
#endif

				SubState = (uint8_t)ServerAwaitingLinkEnum::SwitchingToLinking;
				StateTransition.OnStart(micros());
				PreLinkPacketSchedule = millis();
			}
			else
			{
				Session.ResetPke();
				SubState = (uint8_t)ServerAwaitingLinkEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case ServerAwaitingLinkEnum::ComputingSecretKey:
			if (Session.CalculatePke()) {
				// PKE calculation took a lot of time, let's go for another start request, now with cached secrets.
				SubState = (uint8_t)ServerAwaitingLinkEnum::BroadcastingSession;
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
					SubState = (uint8_t)ServerAwaitingLinkEnum::BroadcastingSession;
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

#if defined(DEBUG_LOLA)
				//this->Owner();
				//Serial.print(F("Sending Linking SwitchOver: "));
				//Serial.print(StateTransition.GetDurationUntilTimeOut(micros()));
				//Serial.println(F("us remaining."));
#endif
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
		case ServerLinkingEnum::AuthenticationRequest:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::ServerChallengeRequest::SUB_HEADER_INDEX] = Linking::ServerChallengeRequest::SUB_HEADER;
				Session.CopyLocalChallengeTo(&OutPacket.Payload[Linking::ServerChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending ServerChallengeRequest."));
#endif
				if (SendPacket(OutPacket.Data, Linking::ServerChallengeRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::ServerChallengeRequest::PAYLOAD_SIZE);
				}
			}
			break;
		case ServerLinkingEnum::AuthenticationReply:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::ServerChallengeReply::SUB_HEADER_INDEX] = Linking::ServerChallengeReply::SUB_HEADER;
				Session.SignPartnerChallengeTo(&OutPacket.Payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]);

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending ServerChallengeReply."));
#endif
				if (SendPacket(OutPacket.Data, Linking::ServerChallengeReply::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::ServerChallengeReply::PAYLOAD_SIZE);
				}
			}
			break;
		case ServerLinkingEnum::ClockSyncing:
			if (ClockReplyPending && Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
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

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.print(F("Sending Clock Error:"));
#endif
				if (InEstimateWithinTolerance())
				{
					OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] = UINT8_MAX;
#if defined(DEBUG_LOLA)
					Serial.println(F("Accepted."));
#endif
				}
				else
				{
					OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] = 0;
#if defined(DEBUG_LOLA)
					Serial.println(F("Rejected."));
#endif
				}

				if (SendPacket(OutPacket.Data, Linking::ClockSyncReply::PAYLOAD_SIZE))
				{
					// Only send a time reply once.
					ClockReplyPending = false;
				}
			}
			Task::enableIfNot();
			break;
		case ServerLinkingEnum::SwitchingToLinked:
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
					SubState = (uint8_t)ServerLinkingEnum::ClockSyncing;
					ClockReplyPending = false;
					PreLinkPacketSchedule = millis();
					Task::enable();
				}
			}
			else if (StateTransition.IsSendRequested(micros()) && Transceiver->TxAvailable())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::LinkTimedSwitchOver::SUB_HEADER_INDEX] = Linking::LinkTimedSwitchOver::SUB_HEADER;
				StateTransition.CopyDurationUntilTimeOutTo(micros() + GetSendDuration(Linking::LinkTimedSwitchOver::PAYLOAD_SIZE), &OutPacket.Payload[Linking::LinkTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				
#if defined(DEBUG_LOLA)
				//this->Owner();
				//Serial.print(F("Sending Link SwitchOver: "));
				//Serial.print(StateTransition.GetDurationUntilTimeOut(micros()));
				//Serial.println(F("us remaining."));
#endif
				if (SendPacket(OutPacket.Data, Linking::LinkTimedSwitchOver::PAYLOAD_SIZE))
				{
					StateTransition.OnSent(micros());
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