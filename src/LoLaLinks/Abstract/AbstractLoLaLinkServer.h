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

	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;

	static constexpr uint32_t SERVER_SLEEP_TIMEOUT_MILLIS = 30000;

	enum WaitingStateEnum
	{
		Sleeping,
		SearchingLink,
		SessionCreation,
		SwitchingToLinking
	};

	enum ServerLinkingEnum
	{
		AuthenticationRequest,
		AuthenticationReply,
		ClockSyncing,
		SwitchingToLinked
	};

private:

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
	using BaseClass::ResetStageStartTime;
	using BaseClass::GetElapsedMicrosSinceLastUnlinkedSent;

	using BaseClass::RequestSendPacket;
	using BaseClass::CanRequestSend;
	using BaseClass::ResetLastSent;
	using BaseClass::GetElapsedSinceLastSent;
	using BaseClass::GetStageElapsedMillis;

protected:
	ServerTimedStateTransition<
		LoLaLinkDefinition::LINKING_TRANSITION_PERIOD_MICROS,
		LoLaLinkDefinition::LINKING_TRANSITION_RESEND_PERIOD_MICROS>
		StateTransition;

	Timestamp InEstimate{};
	TimestampError EstimateErrorReply{};

private:
	WaitingStateEnum WaitingState = WaitingStateEnum::Sleeping;

	ServerLinkingEnum LinkingState = ServerLinkingEnum::AuthenticationRequest;

	bool SearchReplyPending = false;
	bool ClockReplyPending = false;

protected:
	virtual void OnServiceSessionCreation() {}
	virtual void ResetSessionCreation() {}

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
#if defined(DEBUG_LOLA)
	virtual void Owner() final
	{
		Serial.print(millis());
		Serial.print(F("\t[S] "));
	}
#endif

protected:
	void StartSwitchToLinking()
	{
		StateTransition.OnStart(micros());
		WaitingState = WaitingStateEnum::SwitchingToLinking;
		ResetLastSent();
		Task::enable();
	}

	const bool IsInSessionCreation()
	{
		return WaitingState == WaitingStateEnum::SessionCreation;
	}

	const bool IsInSearchingLink()
	{
		return WaitingState == WaitingStateEnum::SearchingLink;
	}

	void StartSessionCreationIfNot()
	{
		if (WaitingState != WaitingStateEnum::SessionCreation)
		{
			WaitingState = WaitingStateEnum::SessionCreation;
			ResetSessionCreation();
			Task::enable();
		}
	}

	void ServerSleep()
	{
		WaitingState = WaitingStateEnum::Sleeping;
		Task::enable();
	}

protected:
	virtual void OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter)
	{
		switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
		{
		case Unlinked::SearchRequest::SUB_HEADER:
			if (payloadSize == Unlinked::SearchRequest::PAYLOAD_SIZE)
			{
				switch (WaitingState)
				{
				case WaitingStateEnum::Sleeping:
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Broadcast wake up!"));
#endif
					// Wake up!
					WaitingState = WaitingStateEnum::SearchingLink;
					break;
				case WaitingStateEnum::SearchingLink:
					break;
				default:
					return;
					break;
				}
				Task::enable();
				SearchReplyPending = true;
			}
			break;
		case Unlinked::LinkingTimedSwitchOverAck::SUB_HEADER:
			if (payloadSize == Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE
				&& WaitingState == WaitingStateEnum::SwitchingToLinking
				&& Encoder->LinkingTokenMatches(&payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SESSION_TOKEN_INDEX]))
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

	virtual void OnLinkingPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) final
	{
		switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
		{
		case Linking::ClientChallengeReplyRequest::SUB_HEADER:
			if (LinkingState == ServerLinkingEnum::AuthenticationRequest
				&& payloadSize == Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE
				&& Encoder->VerifyChallengeSignature(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]))
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Link Access Control granted."));
#endif

				Encoder->SetPartnerChallenge(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);
				LinkingState = ServerLinkingEnum::AuthenticationReply;
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
				switch (LinkingState)
				{
				case ServerLinkingEnum::ClockSyncing:
					break;
				case ServerLinkingEnum::AuthenticationReply:
					LinkingState = ServerLinkingEnum::ClockSyncing;
					Task::enable();
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Started Clock sync"));
#endif
					break;
				case ServerLinkingEnum::SwitchingToLinked:
					LinkingState = ServerLinkingEnum::ClockSyncing;
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

				if (!InEstimate.Validate())
				{
					// Invalid estimate, sub-seconds should never match one second.
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Clock sync Invalid estimate. SubSeconds="));
					Serial.print(InEstimate.SubSeconds);
					Serial.println(F("us. "));
#endif
					ClockReplyPending = false;
					break;
				}

				SyncClock.GetTimestamp(LinkTimestamp);
				LinkTimestamp.ShiftSubSeconds(startTimestamp - micros());
				EstimateErrorReply.CalculateError(LinkTimestamp, InEstimate);

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.print(F("\tClock Sync Error: "));
				Serial.println(EstimateErrorReply.ErrorMicros());
#endif
				ClockReplyPending = true;
				Task::enable();
			}
			break;
		case Linking::StartLinkRequest::SUB_HEADER:
			if (payloadSize == Linking::StartLinkRequest::PAYLOAD_SIZE
				&& LinkingState == ServerLinkingEnum::ClockSyncing)
			{

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Got Link Start Request."));
#endif
				LinkingState = ServerLinkingEnum::SwitchingToLinked;
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
				&& LinkingState == ServerLinkingEnum::SwitchingToLinked)
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

	virtual void OnPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
		if (port == Linked::PORT)
		{
			switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
			{
			case Linked::ClockTuneMicrosRequest::SUB_HEADER:
				if (payloadSize == Linked::ClockTuneMicrosRequest::PAYLOAD_SIZE
					&& !ClockReplyPending)
				{
					// Re-use linking-time clock sync holders.
					InEstimate.SubSeconds = payload[Linked::ClockTuneMicrosRequest::PAYLOAD_ROLLING_INDEX];
					InEstimate.SubSeconds += (uint_least16_t)payload[Linked::ClockTuneMicrosRequest::PAYLOAD_ROLLING_INDEX + 1] << 8;
					InEstimate.SubSeconds += (uint32_t)payload[Linked::ClockTuneMicrosRequest::PAYLOAD_ROLLING_INDEX + 2] << 16;
					InEstimate.SubSeconds += (uint32_t)payload[Linked::ClockTuneMicrosRequest::PAYLOAD_ROLLING_INDEX + 3] << 24;

					SyncClock.GetTimestamp(LinkTimestamp);
					LinkTimestamp.ShiftSubSeconds(startTimestamp - micros());
					EstimateErrorReply.SubSeconds = LinkTimestamp.GetRollingMicros();
					EstimateErrorReply.SubSeconds -= InEstimate.SubSeconds;
					EstimateErrorReply.Seconds = 0;

					ClockReplyPending = true;
					Task::enable();
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
			break;
		case LinkStageEnum::AwaitingLink:
			SetHopperFixedChannel(RandomSource.GetRandomShort());
			NewSession();
			WaitingState = WaitingStateEnum::Sleeping;
			break;
		case LinkStageEnum::Linking:
			Encoder->GenerateLocalChallenge(&RandomSource);
			LinkingState = ServerLinkingEnum::AuthenticationRequest;
			ClockReplyPending = false;
			break;
		case LinkStageEnum::Linked:
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
			ResetStageStartTime();
			Task::disable();
			break;
		case WaitingStateEnum::SearchingLink:
			if (GetStageElapsedMillis() > SERVER_SLEEP_TIMEOUT_MILLIS)
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
			if (GetStageElapsedMillis() > SERVER_SLEEP_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("OnAwaitingLink timed out. Going to sleep."));
#endif
				ServerSleep();
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
		if (GetStageElapsedMillis() > LoLaLinkDefinition::LINKING_STAGE_TIMEOUT)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("Linking timed out!"));
#endif
			UpdateLinkStage(LinkStageEnum::AwaitingLink);
			return;
		}

		switch (LinkingState)
		{
		case ServerLinkingEnum::AuthenticationRequest:
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
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
				}
			}
			break;
		case ServerLinkingEnum::AuthenticationReply:
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
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
				}
			}
			break;
		case ServerLinkingEnum::ClockSyncing:
			if (ClockReplyPending && PacketService.CanSendPacket())
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
					LinkingState = ServerLinkingEnum::ClockSyncing;
					ClockReplyPending = false;
					Task::enable();
				}
			}
			else if (StateTransition.IsSendRequested(micros()) && PacketService.CanSendPacket())
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

	virtual const bool CheckForClockSyncUpdate() final
	{
		if (ClockReplyPending)
		{
			if (CanRequestSend())
			{
				OutPacket.SetPort(Linked::PORT);
				OutPacket.Payload[Linked::ClockTuneMicrosReply::SUB_HEADER_INDEX] = Linked::ClockTuneMicrosReply::SUB_HEADER;
				OutPacket.Payload[Linked::ClockTuneMicrosReply::PAYLOAD_ERROR_INDEX + 0] = EstimateErrorReply.SubSeconds;
				OutPacket.Payload[Linked::ClockTuneMicrosReply::PAYLOAD_ERROR_INDEX + 1] = EstimateErrorReply.SubSeconds >> 8;
				OutPacket.Payload[Linked::ClockTuneMicrosReply::PAYLOAD_ERROR_INDEX + 2] = EstimateErrorReply.SubSeconds >> 16;
				OutPacket.Payload[Linked::ClockTuneMicrosReply::PAYLOAD_ERROR_INDEX + 3] = EstimateErrorReply.SubSeconds >> 24;

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.print(F("Sending Clock Tune Error:"));
				Serial.print((int32_t)EstimateErrorReply.SubSeconds);
				Serial.println(F("us."));
#endif
				if (RequestSendPacket(Linked::ClockTuneMicrosReply::PAYLOAD_SIZE))
				{
					// Only send a time reply once.
					ClockReplyPending = false;
				}
			}

			return true;
		}

		return false;
	}

private:
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

	void NewSession()
	{
		Encoder->SetRandomSessionId(&RandomSource);
		SyncClock.ShiftSeconds(RandomSource.GetRandomLong());
	}

	void OnServiceSearchingLink()
	{
		switch (WaitingState)
		{
		case WaitingStateEnum::Sleeping:
			Task::disable();
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("Search time out, going to sleep."));
#endif
			break;
		case WaitingStateEnum::SearchingLink:
			if (SearchReplyPending)
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::SearchReply::SUB_HEADER_INDEX] = Unlinked::SearchReply::SUB_HEADER;
				if (SendPacket(OutPacket.Data, Unlinked::SearchReply::PAYLOAD_SIZE))
				{
					SearchReplyPending = false;
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Search Reply."));
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
		default:
			return;
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
				// No Ack before time out.
				WaitingState = WaitingStateEnum::SearchingLink;
				Task::enable();
			}
		}
		else if (StateTransition.IsSendRequested(micros())
			&& GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
			&& PacketService.CanSendPacket())
		{
			OutPacket.SetPort(Unlinked::PORT);
			OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::SUB_HEADER_INDEX] = Unlinked::LinkingTimedSwitchOver::SUB_HEADER;
			Encoder->CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]);
			StateTransition.CopyDurationUntilTimeOutTo(micros() + GetSendDuration(Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE), &OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_TIME_INDEX]);

			if (SendPacket(OutPacket.Data, Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE))
			{
				StateTransition.OnSent(micros());
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sent LinkingTimedSwitchOver."));
#endif
			}
		}
		else
		{
			Task::enableIfNot();
		}
	}
};
#endif