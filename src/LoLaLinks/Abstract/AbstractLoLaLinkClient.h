// AbstractLoLaLinkClient.h

#ifndef _ABSTRACT_LOLA_LINK_CLIENT_
#define _ABSTRACT_LOLA_LINK_CLIENT_

#include "..\Abstract\AbstractLoLaLink.h"

template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLaLinkClient : public AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;

	using Linking = LoLaLinkDefinition::Linking;

	static constexpr uint32_t CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS = 50;

	enum ClientLinkingEnum
	{
		WaitingForAuthenticationRequest,
		AuthenticationReply,
		AuthenticationRequest,
		ClockSyncing,
		RequestingLinkStart,
		SwitchingToLinked
	};

protected:
	using BaseClass::OutPacket;
	using BaseClass::Encoder;
	using BaseClass::SyncClock;
	using BaseClass::RandomSource;
	using BaseClass::Transceiver;

	using BaseClass::GetElapsedSinceLastValidReceived;
	using BaseClass::GetSendDuration;
	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;
	using BaseClass::PreLinkResendDelayMillis;

protected:
	ClientTimedStateTransition<
		LoLaLinkDefinition::LINKING_TRANSITION_PERIOD_MICROS,
		LoLaLinkDefinition::LINKING_TRANSITION_RESEND_PERIOD_MICROS>
		StateTransition;

	Timestamp OutEstimate{};
	TimestampError EstimateErrorReply{};

	uint32_t PreLinkPacketSchedule = 0;
	uint8_t SubState = 0;

protected:
	uint8_t SearchChannel = 0;
	uint8_t LastKnownBroadCastChannel = INT8_MAX + 1;

public:
	AbstractLoLaLinkClient(Scheduler& scheduler,
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
		switch (payload[Linking::ServerChallengeRequest::SUB_HEADER_INDEX])
		{
		case Linking::ServerChallengeRequest::SUB_HEADER:
			if (payloadSize == Linking::ServerChallengeRequest::PAYLOAD_SIZE)
			{
				switch (SubState)
				{
				case ClientLinkingEnum::WaitingForAuthenticationRequest:
					Encoder->SetPartnerChallenge(&payload[Linking::ServerChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);
					SubState = ClientLinkingEnum::AuthenticationReply;
					PreLinkPacketSchedule = millis();
					Task::enable();
					break;
				case ClientLinkingEnum::AuthenticationReply:
					break;
				default:
					break;
				}
			}
#if defined(DEBUG_LOLA)
			else
			{
				this->Owner();
				Serial.println(F("ServerChallengeRequest rejected."));
			}
#endif
			break;
		case Linking::ServerChallengeReply::SUB_HEADER:
			if (payloadSize == Linking::ServerChallengeReply::PAYLOAD_SIZE)
			{
				switch (SubState)
				{
				case ClientLinkingEnum::AuthenticationReply:
					if (Encoder->VerifyChallengeSignature(&payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Link Access Control granted."));
#endif
						PreLinkPacketSchedule = millis();
						SubState = ClientLinkingEnum::ClockSyncing;
						Task::enable();
					}
					break;
				case ClientLinkingEnum::ClockSyncing:
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Unexpected Access Control response."));
#endif
					break;
				default:
					break;
				}
			}
			break;
		case Linking::ClockSyncReply::SUB_HEADER:
			if (payloadSize == Linking::ClockSyncReply::PAYLOAD_SIZE
				&& SubState == ClientLinkingEnum::ClockSyncing)
			{
				EstimateErrorReply.Seconds = payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX];
				EstimateErrorReply.Seconds += (uint_least16_t)payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 1] << 8;
				EstimateErrorReply.Seconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 2] << 16;
				EstimateErrorReply.Seconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 3] << 24;
				EstimateErrorReply.SubSeconds = payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX];
				EstimateErrorReply.SubSeconds += (uint_least16_t)payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 1] << 8;
				EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 2] << 16;
				EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 3] << 24;

				if (!EstimateErrorReply.Validate())
				{
#if defined(DEBUG_LOLA)
					// Invalid estimate, sub-seconds should never match one second.
					this->Owner();
					Serial.print(F("Clock Estimate Error Invalid. SubSeconds="));
					Serial.print(EstimateErrorReply.SubSeconds);
					Serial.println(F("us. "));
#endif
				}
				// Adjust local clock to match estimation error.
				SyncClock.ShiftSeconds(EstimateErrorReply.Seconds);
				SyncClock.ShiftMicros(EstimateErrorReply.SubSeconds);

				if (payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] > 0)
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Clock Accepted, starting final step."));
#endif
					SubState = ClientLinkingEnum::RequestingLinkStart;
					StateTransition.OnStart();
					PreLinkPacketSchedule = millis();
				}
				else
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Clock Rejected, trying again."));
#endif
				}

				PreLinkPacketSchedule = millis();
				Task::enable();
			}
			break;
		case Linking::LinkTimedSwitchOver::SUB_HEADER:
			if (payloadSize == Linking::LinkTimedSwitchOver::PAYLOAD_SIZE)
			{
				switch (SubState)
				{
				case ClientLinkingEnum::RequestingLinkStart:
					SubState = (uint8_t)ClientLinkingEnum::SwitchingToLinked;
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("StateTransition Client received, started with"));
					Serial.print(StateTransition.GetDurationUntilTimeOut(startTimestamp));
					Serial.println(F("us remaining."));
#endif
					break;
				case ClientLinkingEnum::SwitchingToLinked:
					break;
				default:
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("LinkSwitchOver rejected."));
#endif
					return;
					break;
				}
				StateTransition.OnReceived(startTimestamp, &payload[Linking::LinkTimedSwitchOver::PAYLOAD_TIME_INDEX]);

				Task::enable();
			}
#if defined(DEBUG_LOLA)
			else
			{
				this->Owner();
				Serial.println(F("LinkSwitchOver rejected."));
			}
#endif
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
			Encoder->SetRandomSessionId(&RandomSource);
			SyncClock.ShiftSeconds(RandomSource.GetRandomLong());
			LastKnownBroadCastChannel = RandomSource.GetRandomShort();
			break;
		case LinkStageEnum::AwaitingLink:
			SearchChannel = LastKnownBroadCastChannel;
			SetHopperFixedChannel(SearchChannel);
			break;
		case LinkStageEnum::Linking:
			Encoder->GenerateLocalChallenge(&RandomSource);
			SubState = (uint8_t)ClientLinkingEnum::WaitingForAuthenticationRequest;
			break;
		case LinkStageEnum::Linked:
			LastKnownBroadCastChannel = SearchChannel;
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
		case ClientLinkingEnum::WaitingForAuthenticationRequest:
			// Server will be the one initiating the authentication step.
			if (stateElapsed > CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("WaitingForAuthenticationRequest timed out!"));
#endif
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else
			{
				Task::enableDelayed(1);
			}
			break;
		case ClientLinkingEnum::AuthenticationReply:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::ClientChallengeReplyRequest::SUB_HEADER_INDEX] = Linking::ClientChallengeReplyRequest::SUB_HEADER;
				Encoder->SignPartnerChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]);
				Encoder->CopyLocalChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending ClientChallengeReplyRequest."));
#endif

				if (SendPacket(OutPacket.Data, Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE);
				}
				else
				{
					Task::enable();
				}
			}
			else
			{
				Task::enableDelayed(1);
			}
			break;
		case ClientLinkingEnum::ClockSyncing:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::ClockSyncRequest::SUB_HEADER_INDEX] = Linking::ClockSyncRequest::SUB_HEADER;

				SyncClock.GetTimestamp(OutEstimate);
				OutEstimate.ShiftSubSeconds((int32_t)GetSendDuration(Linking::ClockSyncRequest::PAYLOAD_SIZE));

				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX] = OutEstimate.Seconds;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 1] = OutEstimate.Seconds >> 8;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 2] = OutEstimate.Seconds >> 16;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 3] = OutEstimate.Seconds >> 24;

				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX] = OutEstimate.SubSeconds;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 1] = OutEstimate.SubSeconds >> 8;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 2] = OutEstimate.SubSeconds >> 16;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 3] = OutEstimate.SubSeconds >> 24;

				if (SendPacket(OutPacket.Data, Linking::ClockSyncRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Sent ClockSync "));
					Serial.print(OutEstimate.Seconds);
					Serial.println('s');
#endif
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::ClockSyncRequest::PAYLOAD_SIZE);
				}
				else
				{
					Task::enable();
				}
			}
			else
			{
				Task::enableDelayed(1);
			}
			break;
		case ClientLinkingEnum::RequestingLinkStart:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::StartLinkRequest::SUB_HEADER_INDEX] = Linking::StartLinkRequest::SUB_HEADER;

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending Linking Start Request."));
#endif
				if (SendPacket(OutPacket.Data, Linking::StartLinkRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::StartLinkRequest::PAYLOAD_SIZE);
				}
				else
				{
					Task::enableIfNot();
				}
			}
			else
			{
				Task::enableIfNot();
			}
			break;
		case ClientLinkingEnum::SwitchingToLinked:
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
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("StateTransition timed out."));
#endif
					UpdateLinkStage(LinkStageEnum::AwaitingLink);
				}
			}
			else if (Transceiver->TxAvailable() && StateTransition.IsSendRequested(micros()))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::LinkTimedSwitchOverAck::SUB_HEADER_INDEX] = Linking::LinkTimedSwitchOverAck::SUB_HEADER;

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending StateTransition Ack."));
#endif
				if (SendPacket(OutPacket.Data, Linking::LinkTimedSwitchOverAck::PAYLOAD_SIZE))
				{
					StateTransition.OnSent(micros());
				}
				else
				{
					Task::enableIfNot();
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

#if defined(CLIENT_DROP_LINK_TEST)
	virtual const bool CheckForClockSyncUpdate() final
	{
		if (this->GetLinkDuration() > CLIENT_DROP_LINK_TEST)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.print(("Test disconnect after "));
			Serial.print(CLIENT_DROP_LINK_TEST);
			Serial.println((" seconds."));
#endif
			UpdateLinkStage(LinkStageEnum::AwaitingLink);
		}

		return false;
	}
#endif
};
#endif