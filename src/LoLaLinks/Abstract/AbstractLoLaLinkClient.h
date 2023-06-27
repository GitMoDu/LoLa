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
	using Linked = LoLaLinkDefinition::Linked;

	static constexpr uint32_t CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS = 50;
	static constexpr uint8_t CLOCK_TUNE_FILTER_RATIO = 110;

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
	using BaseClass::PacketService;
	using BaseClass::LinkTimestamp;

	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;
	using BaseClass::GetSendDuration;
	using BaseClass::GetElapsedMicrosSinceLastUnlinkedSent;

	using BaseClass::RequestSendPacket;
	using BaseClass::CanRequestSend;
	using BaseClass::GetElapsedSinceLastSent;
	using BaseClass::GetStageElapsedMillis;

protected:
	ClientTimedStateTransition<
		LoLaLinkDefinition::LINKING_TRANSITION_PERIOD_MICROS,
		LoLaLinkDefinition::LINKING_TRANSITION_RESEND_PERIOD_MICROS>
		StateTransition;

	TimestampError EstimateErrorReply{};


protected:
	uint8_t SearchChannel = 0;
	uint8_t LastKnownBroadCastChannel = INT8_MAX + 1;

private:
	int32_t ClockErrorFiltered = 0;
	uint32_t LastCLockSync = 0;
	uint32_t LastCLockSent = 0;

	ClientLinkingEnum SubState = ClientLinkingEnum::WaitingForAuthenticationRequest;

	bool WaitingForClockReply = 0;

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
				}
				else
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Clock Rejected, trying again."));
#endif
				}
				Task::enable();
			}
			break;
		case Linking::LinkTimedSwitchOver::SUB_HEADER:
			if (payloadSize == Linking::LinkTimedSwitchOver::PAYLOAD_SIZE)
			{
				switch (SubState)
				{
				case ClientLinkingEnum::RequestingLinkStart:
					SubState = ClientLinkingEnum::SwitchingToLinked;
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("StateTransition Client received, started with "));
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

	virtual void OnPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
		if (port == Linked::PORT)
		{
			switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
			{
			case Linked::ClockTuneMicrosReply::SUB_HEADER:
				if (payloadSize == Linked::ClockTuneMicrosReply::PAYLOAD_SIZE
					&& WaitingForClockReply)
				{
					// Re-use linking-time estimate holder.
					EstimateErrorReply.Seconds = 0;
					EstimateErrorReply.SubSeconds = payload[Linked::ClockTuneMicrosReply::PAYLOAD_ERROR_INDEX];
					EstimateErrorReply.SubSeconds += (uint_least16_t)payload[Linked::ClockTuneMicrosReply::PAYLOAD_ERROR_INDEX + 1] << 8;
					EstimateErrorReply.SubSeconds += (uint32_t)payload[Linked::ClockTuneMicrosReply::PAYLOAD_ERROR_INDEX + 2] << 16;
					EstimateErrorReply.SubSeconds += (uint32_t)payload[Linked::ClockTuneMicrosReply::PAYLOAD_ERROR_INDEX + 3] << 24;

					if (((int32_t)EstimateErrorReply.SubSeconds >= 0 && (int32_t)EstimateErrorReply.SubSeconds < LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
						|| ((int32_t)EstimateErrorReply.SubSeconds < 0 && (int32_t)EstimateErrorReply.SubSeconds > -(int32_t)LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS))
					{
						// Due to error limit of LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS, operation doesn't need to be elevated to uint64_t.
						ClockErrorFiltered += (((int32_t)EstimateErrorReply.SubSeconds - ClockErrorFiltered) * CLOCK_TUNE_FILTER_RATIO) / UINT8_MAX;

						// Adjust client clock with filtered estimation error.
						if (ClockErrorFiltered != 0)
						{
							SyncClock.ShiftMicros(ClockErrorFiltered);
						}

						LastCLockSync = millis();
					}
					else
					{
						// Error too big, discarding.
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.print(F("Clock Tune rejected:"));
						Serial.print((int32_t)EstimateErrorReply.SubSeconds);
						Serial.println(F("us."));
#endif
					}

					WaitingForClockReply = false;
				}
				break;
			default:
				BaseClass::OnPacketReceived(startTimestamp, payload, payloadSize, port);
				break;
			}
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
			SubState = ClientLinkingEnum::WaitingForAuthenticationRequest;
			break;
		case LinkStageEnum::Linked:
			LastKnownBroadCastChannel = SearchChannel;
			ClockErrorFiltered = 0;
			WaitingForClockReply = false;
			LastCLockSync = millis();
			LastCLockSent = millis();
			break;
		default:
			break;
		}
	}

	virtual void OnServiceLinking() final
	{
		if (GetStageElapsedMillis() > LoLaLinkDefinition::LINKING_STAGE_TIMEOUT)
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
			if (GetStageElapsedMillis() > CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("WaitingForAuthenticationRequest timed out!"));
#endif
				// Server will be the one initiating the authentication step.
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else
			{
				Task::enableDelayed(1);
			}
			break;
		case ClientLinkingEnum::AuthenticationReply:
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::ClientChallengeReplyRequest::SUB_HEADER_INDEX] = Linking::ClientChallengeReplyRequest::SUB_HEADER;
				Encoder->SignPartnerChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]);
				Encoder->CopyLocalChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);

				if (SendPacket(OutPacket.Data, Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent ClientChallengeReplyRequest."));
#endif
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
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::ClockSyncRequest::SUB_HEADER_INDEX] = Linking::ClockSyncRequest::SUB_HEADER;

				SyncClock.GetTimestamp(LinkTimestamp);
				LinkTimestamp.ShiftSubSeconds((int32_t)GetSendDuration(Linking::ClockSyncRequest::PAYLOAD_SIZE));

				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX] = LinkTimestamp.Seconds;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 1] = LinkTimestamp.Seconds >> 8;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 2] = LinkTimestamp.Seconds >> 16;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 3] = LinkTimestamp.Seconds >> 24;

				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX] = LinkTimestamp.SubSeconds;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 1] = LinkTimestamp.SubSeconds >> 8;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 2] = LinkTimestamp.SubSeconds >> 16;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 3] = LinkTimestamp.SubSeconds >> 24;

				if (SendPacket(OutPacket.Data, Linking::ClockSyncRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Sent ClockSync "));
					Serial.print(LinkTimestamp.Seconds);
					Serial.println('s');
#endif
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
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::StartLinkRequest::SUB_HEADER_INDEX] = Linking::StartLinkRequest::SUB_HEADER;

				if (SendPacket(OutPacket.Data, Linking::StartLinkRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent RequestingLinkStart."));
#endif
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
			else if (PacketService.CanSendPacket() && StateTransition.IsSendRequested(micros()))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::LinkTimedSwitchOverAck::SUB_HEADER_INDEX] = Linking::LinkTimedSwitchOverAck::SUB_HEADER;

				if (SendPacket(OutPacket.Data, Linking::LinkTimedSwitchOverAck::PAYLOAD_SIZE))
				{
					StateTransition.OnSent(micros());
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent StateTransition Ack."));
#endif
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

	virtual void OnPreSend()
	{
		if (OutPacket.GetPort() == Linked::PORT &&
			OutPacket.Payload[Linked::ClockTuneMicrosRequest::SUB_HEADER_INDEX] == Linked::ClockTuneMicrosRequest::SUB_HEADER)
		{
			const int32_t sendDuration = GetSendDuration(Linked::ClockTuneMicrosRequest::PAYLOAD_SIZE);

			SyncClock.GetTimestamp(LinkTimestamp);
			LinkTimestamp.ShiftSubSeconds(sendDuration);

			const uint32_t rolling = LinkTimestamp.GetRollingMicros();

			OutPacket.Payload[Linked::ClockTuneMicrosRequest::PAYLOAD_ROLLING_INDEX] = rolling;
			OutPacket.Payload[Linked::ClockTuneMicrosRequest::PAYLOAD_ROLLING_INDEX + 1] = rolling >> 8;
			OutPacket.Payload[Linked::ClockTuneMicrosRequest::PAYLOAD_ROLLING_INDEX + 2] = rolling >> 16;
			OutPacket.Payload[Linked::ClockTuneMicrosRequest::PAYLOAD_ROLLING_INDEX + 3] = rolling >> 24;
		}
	}

	virtual const bool CheckForClockSyncUpdate() final
	{
		if (WaitingForClockReply)
		{
			if (millis() - LastCLockSent > LoLaLinkDefinition::CLOCK_TUNE_RETRY_PERIOD)
			{
				WaitingForClockReply = false;
				// Wait for reply before trying again.
				return true;
			}
		}
		else if (millis() - LastCLockSync > LoLaLinkDefinition::CLOCK_TUNE_PERIOD && CanRequestSend())
		{
			OutPacket.SetPort(Linked::PORT);
			OutPacket.Payload[Linked::ClockTuneMicrosRequest::SUB_HEADER_INDEX] = Linked::ClockTuneMicrosRequest::SUB_HEADER;
			if (RequestSendPacket(Linked::ClockTuneMicrosRequest::PAYLOAD_SIZE))
			{
				WaitingForClockReply = true;
				LastCLockSent = millis();
			}
			return true;
		}

		return false;
	}
};
#endif