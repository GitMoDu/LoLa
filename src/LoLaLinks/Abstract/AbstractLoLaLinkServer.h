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

	enum LinkingStateEnum
	{
		AuthenticationRequest,
		AuthenticationReply,
		ClockSyncing,
		SwitchingToLinked
	};

private:

protected:
	using BaseClass::OutPacket;
	using BaseClass::Duplex;
	using BaseClass::Encoder;
	using BaseClass::SyncClock;
	using BaseClass::RandomSource;
	using BaseClass::PacketService;
	using BaseClass::LinkTimestamp;

	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;
	using BaseClass::GetSendDuration;
	using BaseClass::GetOnAirDuration;
	using BaseClass::ResetStageStartTime;
	using BaseClass::ResetUnlinkedPacketThrottle;
	using BaseClass::UnlinkedPacketThrottle;

	using BaseClass::RequestSendPacket;
	using BaseClass::CanRequestSend;
	using BaseClass::GetStageElapsedMillis;
	using BaseClass::GetPreLinkDuplexPeriod;

	using BaseClass::UInt32ToArray;
	using BaseClass::ArrayToUInt32;
	using BaseClass::Int32ToArray;
	using BaseClass::ArrayToInt32;

private:
	const uint16_t PreLinkDuplexPeriod;
	const uint16_t PreLinkDuplexStart;
	const uint16_t PreLinkDuplexEnd;

protected:
	ServerTimedStateTransition<LoLaLinkDefinition::LINKING_TRANSITION_PERIOD_MICROS> StateTransition;

	Timestamp InEstimate{};
	TimestampError EstimateErrorReply{};

private:
	WaitingStateEnum WaitingState = WaitingStateEnum::Sleeping;

	LinkingStateEnum LinkingState = LinkingStateEnum::AuthenticationRequest;

	uint8_t SyncSequence = 0;

	bool SearchReplyPending = false;
	bool ClockReplyPending = false;

protected:
	virtual void OnServiceSearchingLink() {}
	virtual void OnServiceSessionCreation() {}
	virtual void ResetSessionCreation() {}

public:
	AbstractLoLaLinkServer(Scheduler& scheduler,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, encoder, transceiver, clockSource, timerSource, entropy, duplex, hop)
		, PreLinkDuplexPeriod(GetPreLinkDuplexPeriod(duplex, transceiver))
		, PreLinkDuplexStart(0)
		, PreLinkDuplexEnd(GetPreLinkDuplexEnd(duplex, transceiver))
	{
	}

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
		WaitingState = WaitingStateEnum::SwitchingToLinking;
		StateTransition.OnStart(micros());
		ResetUnlinkedPacketThrottle();
		Task::enable();
	}

	void StartSearching()
	{
		WaitingState = WaitingStateEnum::SearchingLink;
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
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::SearchRequest::HEADER:
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
					ResetStageStartTime();
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
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("SearchRequest"));
			}
#endif
			break;
		case Unlinked::LinkingTimedSwitchOverAck::HEADER:
			if (payloadSize == Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE
				&& WaitingState == WaitingStateEnum::SwitchingToLinking
				&& Encoder->LinkingTokenMatches(&payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SESSION_TOKEN_INDEX]))
			{
				if (SyncSequence != payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX])
				{
#if defined(DEBUG_LOLA)
					this->Skipped(F("LinkingTimedSwitchOverAck Bad SyncSequence."));
#endif
					return;
				}

				StateTransition.OnReceived();
				Task::enable();

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.print(F("StateTransition Server got Ack: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(startTimestamp));
				Serial.println(F("us remaining."));
#endif				
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("LinkingTimedSwitchOverAck"));
			}
#endif
			break;
		default:
			break;
		}
	}

	virtual void OnLinkingPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Linking::ClientChallengeReplyRequest::HEADER:
			if (LinkingState == LinkingStateEnum::AuthenticationRequest
				&& payloadSize == Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE
				&& Encoder->VerifyChallengeSignature(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]))
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Link Access Control granted."));
#endif

				Encoder->SetPartnerChallenge(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);
				LinkingState = LinkingStateEnum::AuthenticationReply;
				Task::enable();
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("ClientChallengeReplyRequest"));
			}
#endif
			break;
		case Linking::ClockSyncRequest::HEADER:
			if (payloadSize == Linking::ClockSyncRequest::PAYLOAD_SIZE)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Got ClockSyncRequest."));
#endif
				switch (LinkingState)
				{
				case LinkingStateEnum::ClockSyncing:
					break;
				case LinkingStateEnum::AuthenticationReply:
					LinkingState = LinkingStateEnum::ClockSyncing;
					Task::enable();
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Started Clock sync"));
#endif
					break;
				case LinkingStateEnum::SwitchingToLinked:
					LinkingState = LinkingStateEnum::ClockSyncing;
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Re-Started Clock sync"));
#endif
					break;
				default:
					return;
					break;
				}

				SyncSequence = payload[Linking::ClockSyncRequest::PAYLOAD_REQUEST_ID_INDEX];
				InEstimate.Seconds = ArrayToUInt32(&payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX]);
				InEstimate.SubSeconds = ArrayToUInt32(&payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX]);

				if (!InEstimate.Validate())
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Clock sync Invalid estimate."));
#endif
					return;
				}

				SyncClock.GetTimestamp(LinkTimestamp);
				LinkTimestamp.ShiftSubSeconds((int32_t)(startTimestamp - micros()));
				EstimateErrorReply.CalculateError(LinkTimestamp, InEstimate);
				ClockReplyPending = true;
				Task::enable();
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("ClockSyncRequest"));
			}
#endif
			break;
		case Linking::StartLinkRequest::HEADER:
			if (payloadSize == Linking::StartLinkRequest::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::ClockSyncing)
			{

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Got Link Start Request."));
#endif
				LinkingState = LinkingStateEnum::SwitchingToLinked;
				StateTransition.OnStart(micros());
				ResetUnlinkedPacketThrottle();
				Task::enableIfNot();
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("StartLinkRequest"));
			}
#endif
			break;
		case Linking::LinkTimedSwitchOverAck::HEADER:
			if (payloadSize == Linking::LinkTimedSwitchOverAck::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::SwitchingToLinked)
			{
				if (SyncSequence != payload[Linking::LinkTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX])
				{
#if defined(DEBUG_LOLA)
					this->Skipped(F("LinkTimedSwitchOverAck Bad SyncSequence."));
#endif
					return;
				}

				StateTransition.OnReceived();
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.print(F("StateTransition Server got Ack: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(startTimestamp));
				Serial.println(F("us remaining."));
#endif
				Task::enable();
			}
#if defined(DEBUG_LOLA)
			else {
				this->Skipped(F("LinkTimedSwitchOverAck"));
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
			case Linked::ClockTuneMicrosRequest::HEADER:
				if (payloadSize == Linked::ClockTuneMicrosRequest::PAYLOAD_SIZE
					&& !ClockReplyPending)
				{
					// Re-use linking-time clock sync holders.
					InEstimate.Seconds = 0;
					InEstimate.SubSeconds = ArrayToUInt32(&payload[Linked::ClockTuneMicrosRequest::PAYLOAD_ROLLING_INDEX]);

					SyncClock.GetTimestamp(LinkTimestamp);
					LinkTimestamp.ShiftSubSeconds(startTimestamp - micros());
					EstimateErrorReply.SubSeconds = LinkTimestamp.GetRollingMicros();
					EstimateErrorReply.SubSeconds -= InEstimate.SubSeconds;
					EstimateErrorReply.Seconds = 0;

					ClockReplyPending = true;
					Task::enable();
				}
#if defined(DEBUG_LOLA)
				else {
					this->Skipped(F("ClockTuneMicrosRequest"));
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
			break;
		case LinkStageEnum::AwaitingLink:
			SetHopperFixedChannel(RandomSource.GetRandomShort());
			NewSession();
			WaitingState = WaitingStateEnum::Sleeping;
			break;
		case LinkStageEnum::Linking:
			Encoder->GenerateLocalChallenge(&RandomSource);
			LinkingState = LinkingStateEnum::AuthenticationRequest;
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
				OnServiceSearchingLinkInternal();
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
		switch (LinkingState)
		{
		case LinkingStateEnum::AuthenticationRequest:
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.SetHeader(Linking::ServerChallengeRequest::HEADER);
				Encoder->CopyLocalChallengeTo(&OutPacket.Payload[Linking::ServerChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);

				if (UnlinkedCanSendPacket(Linking::ServerChallengeRequest::PAYLOAD_SIZE))
				{
					if (SendPacket(OutPacket.Data, Linking::ServerChallengeRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent ServerChallengeRequest."));
#endif
					}
				}
			}
			Task::enable();
			break;
		case LinkingStateEnum::AuthenticationReply:
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.SetHeader(Linking::ServerChallengeReply::HEADER);
				Encoder->SignPartnerChallengeTo(&OutPacket.Payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]);

				if (UnlinkedCanSendPacket(Linking::ServerChallengeReply::PAYLOAD_SIZE))
				{
					if (SendPacket(OutPacket.Data, Linking::ServerChallengeReply::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent ServerChallengeReply."));
#endif
					}
				}
			}
			Task::enable();
			break;
		case LinkingStateEnum::ClockSyncing:
			if (ClockReplyPending && PacketService.CanSendPacket())
			{
				// Only send a time reply once.
				ClockReplyPending = false;

				OutPacket.SetPort(Linking::PORT);
				OutPacket.SetHeader(Linking::ClockSyncReply::HEADER);
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;

				Int32ToArray(EstimateErrorReply.Seconds, &OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX]);
				Int32ToArray(EstimateErrorReply.SubSeconds, &OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX]);

				if (InEstimateWithinTolerance())
				{
					OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] = UINT8_MAX;
				}
				else
				{
					OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] = 0;
				}

				if (SendPacket(OutPacket.Data, Linking::ClockSyncReply::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("EstimateError: "));
					EstimateErrorReply.print();
					Serial.println();
#endif
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
					// No Ack before time out.
					LinkingState = LinkingStateEnum::ClockSyncing;
					ClockReplyPending = false;
				}
			}
			else if (StateTransition.IsSendRequested(micros())
				&& UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.SetHeader(Linking::LinkTimedSwitchOver::HEADER);

				if (UnlinkedCanSendPacket(Linking::LinkTimedSwitchOver::PAYLOAD_SIZE))
				{
					SyncSequence++;
					OutPacket.Payload[Linking::LinkTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;

					const uint32_t timestamp = micros();
					StateTransition.CopyDurationUntilTimeOutTo(timestamp + GetSendDuration(Linking::LinkTimedSwitchOver::PAYLOAD_SIZE), &OutPacket.Payload[Linking::LinkTimedSwitchOver::PAYLOAD_TIME_INDEX]);
					if (SendPacket(OutPacket.Data, Linking::LinkTimedSwitchOver::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.print(F("Sent LinkTimedSwitchOver, "));
						Serial.print(StateTransition.GetDurationUntilTimeOut(timestamp));
						Serial.println(F("us remaining."));
#endif
					}
				}
			}
			Task::enable();
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
				OutPacket.SetHeader(Linked::ClockTuneMicrosReply::HEADER);
				Int32ToArray(EstimateErrorReply.SubSeconds, &OutPacket.Payload[Linked::ClockTuneMicrosReply::PAYLOAD_ERROR_INDEX]);

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

	void OnServiceSearchingLinkInternal()
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
				OutPacket.SetHeader(Unlinked::SearchReply::HEADER);

				if (PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Unlinked::SearchReply::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent Search Reply."));
#endif
					}
					SearchReplyPending = false;
				}
				Task::enable();
			}
			else
			{
				OnServiceSearchingLink();
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
			}
		}
		else if (StateTransition.IsSendRequested(micros())
			&& UnlinkedPacketThrottle())
		{
			OutPacket.SetPort(Unlinked::PORT);
			OutPacket.SetHeader(Unlinked::LinkingTimedSwitchOver::HEADER);
			Encoder->CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]);

			if (UnlinkedCanSendPacket(Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE))
			{
				SyncSequence++;
				OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;

				const uint32_t timestamp = micros();
				StateTransition.CopyDurationUntilTimeOutTo(timestamp + GetSendDuration(Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE), &OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				if (SendPacket(OutPacket.Data, Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Sent LinkingTimedSwitchOver, "));
					Serial.print(StateTransition.GetDurationUntilTimeOut(timestamp));
					Serial.println(F("us remaining."));
#endif
				}
			}
		}
		Task::enable();
	}

protected:
	/// <summary>
	/// Pre-link half-duplex.
	/// Server follows accurate but random SyncClock.
	/// 1/4 Slot for 2x Duplex duration.
	/// </summary>
	/// <param name="payloadSize"></param>
	/// <returns>True when packet can be sent.</returns>
	const bool UnlinkedCanSendPacket(const uint8_t payloadSize)
	{
		SyncClock.GetTimestamp(LinkTimestamp);
		LinkTimestamp.ShiftSubSeconds(GetSendDuration(payloadSize));

		const uint32_t startTimestamp = LinkTimestamp.GetRollingMicros();

		const uint_fast16_t startRemainder = startTimestamp % (PreLinkDuplexPeriod);
		const uint_fast16_t endRemainder = (startTimestamp + GetOnAirDuration(payloadSize)) % (PreLinkDuplexPeriod);

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

private:
	static const uint16_t GetPreLinkDuplexEnd(IDuplex* duplex, ILoLaTransceiver* transceiver)
	{
		return GetPreLinkDuplexPeriod(duplex, transceiver) / 2;
	}
};
#endif