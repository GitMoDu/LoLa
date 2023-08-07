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

	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;

	static constexpr uint32_t CLIENT_SLEEP_TIMEOUT_MILLIS = 30000;
	static constexpr uint32_t CHANNEL_SEARCH_TRY_COUNT = 3;

	static constexpr uint32_t CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS = 100;
	static constexpr uint8_t CLOCK_TUNE_FILTER_RATIO = 160;

	enum WaitingStateEnum
	{
		Sleeping,
		SearchingLink,
		SessionCreation,
		SwitchingToLinking
	};

	enum LinkingStateEnum
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
	using BaseClass::Duplex;
	using BaseClass::Encoder;
	using BaseClass::SyncClock;
	using BaseClass::RandomSource;
	using BaseClass::PacketService;
	using BaseClass::LinkTimestamp;
	using BaseClass::LinkSendDuration;

	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;
	using BaseClass::GetSendDuration;
	using BaseClass::GetOnAirDuration;
	using BaseClass::ResetUnlinkedPacketThrottle;
	using BaseClass::UnlinkedPacketThrottle;

	using BaseClass::SyncSequence;
	using BaseClass::RequestSendPacket;
	using BaseClass::CanRequestSend;
	using BaseClass::GetStageElapsedMillis;
	using BaseClass::ResetStageStartTime;

private:
	const uint16_t PreLinkDuplexPeriod;
	const uint16_t PreLinkDuplexStart;
	const uint16_t PreLinkDuplexEnd;

protected:
	ClientTimedStateTransition<LoLaLinkDefinition::LINKING_TRANSITION_PERIOD_MICROS> StateTransition;

	TimestampError EstimateErrorReply{};

protected:
	uint8_t SearchChannel = 0;
	uint8_t LastKnownBroadCastChannel = INT8_MAX + 1;

private:
	int32_t ClockErrorFiltered = 0;
	uint32_t LastCLockSync = 0;
	uint32_t LastCLockSent = 0;

	uint32_t PreLinkLastSync = 0;

	WaitingStateEnum WaitingState = WaitingStateEnum::Sleeping;

	LinkingStateEnum LinkingState = LinkingStateEnum::WaitingForAuthenticationRequest;

	uint8_t SearchChannelTryCount = 0;

	bool WaitingForClockReply = 0;

protected:
	virtual void OnServiceSessionCreation() {}
	virtual void ResetSessionCreation() {}

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
		, PreLinkDuplexPeriod(duplex->GetPeriod())
		, PreLinkDuplexStart(duplex->GetPeriod())
		, PreLinkDuplexEnd(PreLinkDuplexStart + (duplex->GetPeriod() / 2))
	{}

protected:
#if defined(DEBUG_LOLA)
	virtual void Owner() final
	{
		Serial.print(micros());
		Serial.print(F("\t[C] "));
	}
#endif

protected:
	const bool IsInSessionCreation()
	{
		return WaitingState == WaitingStateEnum::SessionCreation;
	}

protected:
	virtual void OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter)
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::SearchReply::HEADER:
			if (payloadSize == Unlinked::SearchReply::PAYLOAD_SIZE
				&& WaitingState == WaitingStateEnum::SearchingLink)
			{
				WaitingState = WaitingStateEnum::SessionCreation;
				ResetSessionCreation();
				Task::enable();

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Found a server!"));
#endif
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("SearchReply"));
			}
#endif
			break;
		case Unlinked::LinkingTimedSwitchOver::HEADER:
			if (payloadSize == Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE
				&& Encoder->LinkingTokenMatches(&payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]))
			{
				switch (WaitingState)
				{
				case WaitingStateEnum::SessionCreation:
					WaitingState = WaitingStateEnum::SwitchingToLinking;
				case WaitingStateEnum::SwitchingToLinking:
					break;
				default:
					return;
					break;
				}
				StateTransition.OnReceived(startTimestamp, &payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				SyncSequence = payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX];
				Task::enable();

				OnLinkSyncReceived(startTimestamp);
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.print(F("Got LinkingTimedSwitchOver: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(startTimestamp));
				Serial.println(F("us remaining."));
#endif
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("LinkingTimedSwitchOver"));
			}
#endif
			break;
		default:
			break;
		}
	}

	virtual void OnLinkingPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) final
	{
		switch (payload[Linking::ServerChallengeRequest::HEADER_INDEX])
		{
		case Linking::ServerChallengeRequest::HEADER:
			if (payloadSize == Linking::ServerChallengeRequest::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::WaitingForAuthenticationRequest)
			{
				Encoder->SetPartnerChallenge(&payload[Linking::ServerChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);
				LinkingState = LinkingStateEnum::AuthenticationReply;
				Task::enable();
				OnLinkSyncReceived(startTimestamp);
			}
#if defined(DEBUG_LOLA)
			else
			{
				this->Skipped(F("ServerChallengeRequest."));
			}
#endif
			break;
		case Linking::ServerChallengeReply::HEADER:
			if (payloadSize == Linking::ServerChallengeReply::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::AuthenticationReply
				&& Encoder->VerifyChallengeSignature(&payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]))
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Link Access Control granted."));
#endif
				LinkingState = LinkingStateEnum::ClockSyncing;
				Task::enable();
				OnLinkSyncReceived(startTimestamp);
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("ServerChallengeReply"));
			}
#endif
			break;
		case Linking::ClockSyncReply::HEADER:
			if (payloadSize == Linking::ClockSyncReply::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::ClockSyncing)
			{
				if (SyncSequence != payload[Linking::ClockSyncReply::PAYLOAD_REQUEST_ID_INDEX])
				{
#if defined(DEBUG_LOLA)
					this->Skipped(F("ClockSyncReply Bad SyncSequence."));
#endif
					return;
				}

				EstimateErrorReply.Seconds = payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX];
				EstimateErrorReply.Seconds += (uint_least16_t)payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 1] << 8;
				EstimateErrorReply.Seconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 2] << 16;
				EstimateErrorReply.Seconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 3] << 24;
				EstimateErrorReply.SubSeconds = payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX];
				EstimateErrorReply.SubSeconds += (uint_least16_t)payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 1] << 8;
				EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 2] << 16;
				EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 3] << 24;

				if (EstimateErrorReply.Validate())
				{
					// Adjust local clock to match estimation error.
					SyncClock.ShiftSeconds(EstimateErrorReply.Seconds);
					SyncClock.ShiftMicros(EstimateErrorReply.SubSeconds);

					if (payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] > 0)
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Clock Accepted, starting final step."));
#endif
						StateTransition.Clear();
						LinkingState = LinkingStateEnum::RequestingLinkStart;
						ResetUnlinkedPacketThrottle();
					}
#if defined(DEBUG_LOLA)
					else
					{
						this->Owner();
						Serial.println(F("Clock Rejected, trying again."));
					}
#endif
				}
#if defined(DEBUG_LOLA)
				else
				{
					// Invalid estimate, sub-seconds should never match one second.
					this->Owner();
					Serial.print(F("Clock Estimate Error Invalid. SubSeconds="));
					Serial.print(EstimateErrorReply.SubSeconds);
					Serial.println(F("us. "));
				}
#endif
				Task::enable();
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("ClockSyncReply"));
			}
#endif
			break;
		case Linking::LinkTimedSwitchOver::HEADER:
			if (payloadSize == Linking::LinkTimedSwitchOver::PAYLOAD_SIZE)
			{
				switch (LinkingState)
				{
				case LinkingStateEnum::RequestingLinkStart:
					LinkingState = LinkingStateEnum::SwitchingToLinked;
					break;
				case LinkingStateEnum::SwitchingToLinked:
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
				OnLinkSyncReceived(startTimestamp);
				SyncSequence = payload[Linking::LinkTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX];
				Task::enable();

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.print(F("Got LinkTimedSwitchOver: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(startTimestamp));
				Serial.println(F("us remaining."));
#endif
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("LinkSwitchOver"));
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
			switch (payload[HeaderDefinition::HEADER_INDEX])
			{
			case Linked::ClockTuneMicrosReply::HEADER:
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
					}
					else
					{
						// Error too big, limiting correction.
						if ((int32_t)EstimateErrorReply.SubSeconds >= 0)
						{
							ClockErrorFiltered += ((LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS - ClockErrorFiltered) * CLOCK_TUNE_FILTER_RATIO) / UINT8_MAX;
						}
						else
						{
							ClockErrorFiltered += ((-(int32_t)LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS - ClockErrorFiltered) * CLOCK_TUNE_FILTER_RATIO) / UINT8_MAX;
						}
					}

					// Adjust client clock with filtered estimation error.
					if (ClockErrorFiltered != 0)
					{
						SyncClock.ShiftMicros(ClockErrorFiltered);
					}

					LastCLockSync = millis();

					WaitingForClockReply = false;
				}
#if defined(DEBUG_LOLA)
				else {
					this->Skipped(F("ClockTuneMicrosReply"));
				}
#endif
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
			SearchChannelTryCount = 0;
			StateTransition.Clear();
			ResetUnlinkedPacketThrottle();
			WaitingState = WaitingStateEnum::SearchingLink;
			break;
		case LinkStageEnum::Linking:
			StateTransition.Clear();
			Encoder->GenerateLocalChallenge(&RandomSource);
			LinkingState = LinkingStateEnum::WaitingForAuthenticationRequest;
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

	virtual void OnServiceAwaitingLink() final
	{
		switch (WaitingState)
		{
		case WaitingStateEnum::Sleeping:
			Task::disable();
			break;
		case WaitingStateEnum::SearchingLink:
			if (GetStageElapsedMillis() > CLIENT_SLEEP_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("SearchingLink time out."));
#endif
				WaitingState = WaitingStateEnum::Sleeping;
				Task::enable();
			}
			else
			{
				OnServiceSearchingLink();
			}
			break;
		case WaitingStateEnum::SessionCreation:
			if (GetStageElapsedMillis() > CLIENT_SLEEP_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("SessionCreation timed out. Going back to search for link."));
#endif
				WaitingState = WaitingStateEnum::SearchingLink;
				Task::enable();
			}
			else
			{
				OnServiceSessionCreation();
			}
			break;
		case WaitingStateEnum::SwitchingToLinking:
			OnServiceSwitchingToLinking();
			break;
		default:
			break;
		}
	}

	virtual void OnServiceLinking() final
	{
		switch (LinkingState)
		{
		case LinkingStateEnum::WaitingForAuthenticationRequest:
			if (GetStageElapsedMillis() > CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("WaitingForAuthenticationRequest timed out!"));
#endif
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else
			{
				// Server will be the one initiating the authentication step.
				Task::enableDelayed(1);
			}
			break;
		case LinkingStateEnum::AuthenticationReply:
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.SetHeader(Linking::ClientChallengeReplyRequest::HEADER);
				Encoder->SignPartnerChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]);
				Encoder->CopyLocalChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);

				if (UnlinkedCanSendPacket(Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE))
				{
					if (SendPacket(OutPacket.Data, Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent ClientChallengeReplyRequest."));
#endif
					}
				}
			}
			Task::enable();
			break;
		case LinkingStateEnum::ClockSyncing:
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.SetHeader(Linking::ClockSyncRequest::HEADER);

				LinkSendDuration = (int32_t)GetSendDuration(Linking::ClockSyncRequest::PAYLOAD_SIZE);

				SyncClock.GetTimestamp(LinkTimestamp);
				LinkTimestamp.ShiftSubSeconds(LinkSendDuration);

				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX] = LinkTimestamp.Seconds;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 1] = LinkTimestamp.Seconds >> 8;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 2] = LinkTimestamp.Seconds >> 16;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 3] = LinkTimestamp.Seconds >> 24;

				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX] = LinkTimestamp.SubSeconds;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 1] = LinkTimestamp.SubSeconds >> 8;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 2] = LinkTimestamp.SubSeconds >> 16;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 3] = LinkTimestamp.SubSeconds >> 24;

				if (UnlinkedCanSendPacket(Linking::ClockSyncRequest::PAYLOAD_SIZE))
				{
					SyncSequence++;
					OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;
					if (SendPacket(OutPacket.Data, Linking::ClockSyncRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.print(F("Sent ClockSync "));
						Serial.print(LinkTimestamp.Seconds);
						Serial.println('s');
#endif
					}
				}
			}
			Task::enable();
			break;
		case LinkingStateEnum::RequestingLinkStart:
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.SetHeader(Linking::StartLinkRequest::HEADER);

				if (UnlinkedCanSendPacket(Linking::StartLinkRequest::PAYLOAD_SIZE))
				{
					if (SendPacket(OutPacket.Data, Linking::StartLinkRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent RequestingLinkStart."));
#endif
					}
				}
			}
			Task::enable();
			break;
		case LinkingStateEnum::SwitchingToLinked:
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
			else if (StateTransition.IsSendRequested(micros()))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.SetHeader(Linking::LinkTimedSwitchOverAck::HEADER);
				OutPacket.Payload[Linking::LinkTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;

				if (PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Linking::LinkTimedSwitchOverAck::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent StateTransition Ack."));
#endif
					}
					StateTransition.OnSent();
				}
			}
			Task::enable();
			break;
		default:
			break;
		}
	}

	virtual void OnPreSend()
	{
		if (OutPacket.GetPort() == Linked::PORT &&
			OutPacket.GetHeader() == Linked::ClockTuneMicrosRequest::HEADER)
		{
			LinkSendDuration = GetSendDuration(Linked::ClockTuneMicrosRequest::PAYLOAD_SIZE);

			SyncClock.GetTimestamp(LinkTimestamp);
			LinkTimestamp.ShiftSubSeconds(LinkSendDuration);

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
			OutPacket.SetHeader(Linked::ClockTuneMicrosRequest::HEADER);
			if (RequestSendPacket(Linked::ClockTuneMicrosRequest::PAYLOAD_SIZE))
			{
				WaitingForClockReply = true;
				LastCLockSent = millis();
			}
			return true;
		}

		return false;
	}

private:
	void OnServiceSearchingLink()
	{
		switch (WaitingState)
		{
		case WaitingStateEnum::Sleeping:
			Task::disable();
			break;
		case WaitingStateEnum::SearchingLink:
			if (UnlinkedPacketThrottle())
			{
				if (SearchChannelTryCount >= CHANNEL_SEARCH_TRY_COUNT)
				{
					// Switch channels after a few ms of failed attempt.
					// Has no effect if Channel Hop is permanent.
					SearchChannel++;
					SetHopperFixedChannel(SearchChannel);
					SearchChannelTryCount = 0;
					Task::enable();
				}
				else if (PacketService.CanSendPacket())
				{
					OutPacket.SetPort(Unlinked::PORT);
					OutPacket.SetHeader(Unlinked::SearchRequest::HEADER);

					if (SendPacket(OutPacket.Data, Unlinked::SearchRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent Broadcast Search."));
#endif
					}
					SearchChannelTryCount++;
				}
			}
			Task::enable();
			break;
		default:
			break;
		}
	}

	void OnServiceSwitchingToLinking()
	{
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
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("StateTransition timed out."));
				// No Ack before time out.
				WaitingState = WaitingStateEnum::SearchingLink;

#endif
			}
		}
		else if (StateTransition.IsSendRequested(micros()))
		{
			OutPacket.SetPort(Unlinked::PORT);
			OutPacket.SetHeader(Unlinked::LinkingTimedSwitchOverAck::HEADER);
			OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;
			Encoder->CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SESSION_TOKEN_INDEX]);

			if (PacketService.CanSendPacket())
			{
				if (SendPacket(OutPacket.Data, Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent StateTransition Ack."));
#endif
				}
				StateTransition.OnSent();
			}
		}
		Task::enable();
	}

protected:
	void OnLinkSyncReceived(const uint32_t receiveTimestamp)
	{
		PreLinkLastSync = receiveTimestamp;
	}

	/// <summary>
	/// Pre-link half-duplex.
	/// Client follows last Server duplexed receive timestamp.
	/// 1/4 Slot for 2x Duplex duration.
	/// </summary>
	/// <param name="payloadSize"></param>
	/// <returns>True when packet can be sent.</returns>
	const bool UnlinkedCanSendPacket(const uint8_t payloadSize)
	{
		if (PreLinkDuplexPeriod == IDuplex::DUPLEX_FULL)
		{
			return true;
		}
		else
		{
			const uint32_t startTimestamp = (micros() - PreLinkLastSync) + GetSendDuration(payloadSize);

			const uint_fast16_t startRemainder = startTimestamp % (PreLinkDuplexPeriod * 2);
			const uint_fast16_t endRemainder = (startTimestamp + GetOnAirDuration(payloadSize)) % (PreLinkDuplexPeriod * 2);

			if (endRemainder >= startRemainder
				&& startRemainder > PreLinkDuplexStart
				&& endRemainder < PreLinkDuplexEnd)
			{
				return PacketService.CanSendPacket();
			}
			else
			{
				return false;
			}
		}
	}
};
#endif