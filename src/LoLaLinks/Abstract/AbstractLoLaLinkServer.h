// AbstractLoLaLinkServer.h

#ifndef _ABSTRACT_LOLA_LINK_SERVER_
#define _ABSTRACT_LOLA_LINK_SERVER_

#include "..\Abstract\AbstractLoLaLink.h"

template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLaLinkServer : public AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;
	
	using Linking = LoLaLinkDefinition::Linking;

	enum ServerLinkingEnum
	{
		AuthenticationRequest,
		AuthenticationReply,
		ClockSyncing,
		SwitchingToLinked
	};

protected:
	using BaseClass::OutPacket;
	using BaseClass::Encoder;
	using BaseClass::SyncClock;
	using BaseClass::RandomSource;
	using BaseClass::Transceiver;

	using BaseClass::PreLinkResendDelayMillis;
	using BaseClass::GetSendDuration;
	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;

protected:
	ServerTimedStateTransition<
		LoLaLinkDefinition::LINKING_TRANSITION_PERIOD_MICROS,
		LoLaLinkDefinition::LINKING_TRANSITION_RESEND_PERIOD_MICROS>
		StateTransition;

	uint32_t PreLinkPacketSchedule = 0;
	Timestamp InTimestamp{};
	Timestamp InEstimate{};
	TimestampError EstimateErrorReply{};

	uint8_t SubState = 0;

private:
	bool ClockReplyPending = false;

public:
	AbstractLoLaLinkServer(Scheduler& scheduler,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, encoder, transceiver, entropySource, clockSource, timerSource, duplex, hop)
	{}

protected:
	//virtual void OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) {}

	virtual void OnLinkingPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) final
	{
		switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
		{
		case Linking::ClientChallengeReplyRequest::SUB_HEADER:
			if (SubState == (uint8_t)ServerLinkingEnum::AuthenticationRequest
				&& payloadSize == Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE
				&& Encoder->VerifyChallengeSignature(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]))
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Link Access Control granted."));
#endif

				Encoder->SetPartnerChallenge(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);
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

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
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
			break;
		case LinkStageEnum::Linking:
			Encoder->GenerateLocalChallenge(&RandomSource);
			SubState = (uint8_t)ServerLinkingEnum::AuthenticationRequest;
			ClockReplyPending = false;
			break;
		case LinkStageEnum::Linked:
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
				Encoder->CopyLocalChallengeTo(&OutPacket.Payload[Linking::ServerChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);

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
				Encoder->SignPartnerChallengeTo(&OutPacket.Payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]);

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