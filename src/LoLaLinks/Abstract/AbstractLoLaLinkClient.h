// AbstractLoLaLinkClient.h

#ifndef _ABSTRACT_LOLA_LINK_CLIENT_
#define _ABSTRACT_LOLA_LINK_CLIENT_

#include "AbstractLoLaLink.h"
#include "../../Link/TimedStateTransition.h"
#include "../../Link/LinkClockTracker.h"
#include "../../Link/LinkClockSync.h"
#include "../../Link/PreLinkDuplex.h"

class AbstractLoLaLinkClient : public AbstractLoLaLink
{
private:
	using BaseClass = AbstractLoLaLink;

private:
	static constexpr uint8_t CHANNEL_SEARCH_TRY_COUNT = 3;


protected:
	ClientTimedStateTransition StateTransition;

private:
	PreLinkSlaveDuplex LinkingDuplex;

	LinkClientClockTracker ClockTracker;
	LinkClientClockSync ClockSyncer;

	uint8_t SyncSequence = 0;

	uint8_t SearchChannel = 0;
	uint8_t SearchChannelTryCount = 0;

	bool AuthenticationReplyPending = false;
	bool ClockAccepted = false;

#if defined(DEBUG_LOLA_LINK)
	struct LinkingLogStruct
	{
		uint32_t Searching = 0;
		uint32_t Pairing = 0;
		uint32_t Linking = 0;

		uint32_t Start = 0;

		const uint32_t Link() const
		{
			return Pairing + Linking;
		}
	};

	LinkingLogStruct LinkingLog{};
#endif

public:
	AbstractLoLaLinkClient(Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, linkRegistry, transceiver, cycles, entropy, duplex, hop)
		, StateTransition(LoLaLinkDefinition::GetTransitionDuration(duplex->GetPeriod()))
		, LinkingDuplex(duplex->GetPeriod())
		, ClockTracker(duplex->GetPeriod())
		, ClockSyncer()
	{}

protected:
#if defined(DEBUG_LOLA)
	void Owner() final
	{
		Serial.print(millis());
		Serial.print(F("\t[C] "));
	}
#endif

protected:
	const uint8_t GetClockQuality() final
	{
		return ClockTracker.GetQuality();
	}

	const bool CheckForClockTuneUpdate() final
	{
		if (ClockTracker.HasResultReady())
		{
			// Adjust client clock with filtered estimation error in ns.
			SyncClock.ShiftSubSeconds(ClockTracker.GetResultCorrection());

			// Adjust tune and consume adjustment from filter value.
			SyncClock.ShiftTune(ClockTracker.ConsumeTuneShiftMicros());

			// Set the tracket state to wait for next round of sync.
			ClockTracker.OnResultRead();

#if defined(LOLA_DEBUG_LINK_CLOCK)
			ClockTracker.DebugClockError();
			Serial.print(F("\tTunePPM "));
			Serial.println((int32_t)SyncClock.GetTune());
			Serial.println();
#endif
			return true;
		}
		else if (ClockTracker.HasRequestToSend(micros()))
		{
			if (CanRequestSend())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Linked::ClockTuneRequest::HEADER);

				if (RequestSendPacket(Linked::ClockTuneRequest::PAYLOAD_SIZE, GetClockSyncPriority(ClockTracker.GetQuality())))
				{
					ClockTracker.OnRequestSent(micros());
				}
			}
			return true;
		}

		return false;
	}

	void OnPreSend() final
	{
		if (OutPacket.GetPort() == LoLaLinkDefinition::LINK_PORT
			&& OutPacket.GetHeader() == Linked::ClockTuneRequest::HEADER)
		{
			const uint32_t start = micros();
			const uint32_t timestamp = SyncClock.GetRollingMicros() + GetSendDuration(Linked::ClockTuneRequest::PAYLOAD_SIZE);
			const uint32_t elapsed = micros() - start;
			UInt32ToArray(timestamp + elapsed, &OutPacket.Payload[Linked::ClockTuneRequest::PAYLOAD_ROLLING_INDEX]);
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
			Session.SetRandomSessionId(&RandomSource);
			break;
		case LinkStageEnum::Sleeping:
			break;
		case LinkStageEnum::Searching:
#if defined(DEBUG_LOLA_LINK) 
			LinkingLog.Start = micros();
#endif
			SearchChannel = RandomSource.GetRandomShort();
			SetAdvertisingChannel(SearchChannel);
			SearchChannelTryCount = 0;
			ResetUnlinkedPacketThrottle();
			break;
		case LinkStageEnum::Pairing:
#if defined(DEBUG_LOLA_LINK) 
			LinkingLog.Searching = micros() - LinkingLog.Start;
			LinkingLog.Start = micros();
#endif
			ResetUnlinkedPacketThrottle();
			break;
		case LinkStageEnum::Authenticating:
#if defined(DEBUG_LOLA_LINK) 
			LinkingLog.Pairing = micros() - LinkingLog.Start;
			LinkingLog.Start = micros();
#endif
			AuthenticationReplyPending = false;

			ClockSyncer.Reset(micros());
			ClockTracker.Reset();
			StateTransition.Clear();
			Session.GenerateLocalChallenge(&RandomSource);
			break;
		case LinkStageEnum::ClockSyncing:
			ClockAccepted = false;
			break;
		case LinkStageEnum::Linked:
#if defined(DEBUG_LOLA_LINK)
			LinkingLog.Linking = micros() - LinkingLog.Start;
			LinkingLog.Start = micros();

			Serial.print(F("Link took\t"));
			Serial.print(LinkingLog.Link() / 1000);
			Serial.println(F(" ms to establish"));
			Serial.print(F("\tSearch\t"));
			Serial.print(LinkingLog.Searching / 1000);
			Serial.println(F(" ms"));
			Serial.print(F("\tPairing\t"));
			Serial.print(LinkingLog.Pairing / 1000);
			Serial.println(F(" ms"));
			Serial.print(F("\tLinking\t"));
			Serial.print(LinkingLog.Linking / 1000);
			Serial.println(F(" ms"));
#endif
			ClockTracker.Reset(micros());
			break;
		default:
			break;
		}
	}

	void OnServiceSleeping() final
	{
#if defined(DEBUG_LOLA_LINK)
		this->Owner();
		Serial.println(F("Sleeping."));
#endif
		//TODO: Try to find server but sleep between long tries.
		Task::disable();
	}


	void OnServiceClockSyncing() final
	{
		if (ClockAccepted)
		{
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Linking::StartLinkRequest::HEADER);

				LOLA_RTOS_PAUSE();
				if (UnlinkedDuplexCanSend(Linking::StartLinkRequest::PAYLOAD_SIZE) &&
					PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Linking::StartLinkRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent StartLinkRequest"));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
		}
		else
		{
			if (ClockSyncer.IsTimeToSend(micros(), GetClockSyncRetryPeriod()))
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				if (ClockSyncer.IsBroadAccepted())
				{
					OutPacket.SetHeader(Linking::ClockSyncFineRequest::HEADER);
				}
				else
				{
					OutPacket.SetHeader(Linking::ClockSyncBroadRequest::HEADER);
				}

				LOLA_RTOS_PAUSE();
				if (PacketService.CanSendPacket())
				{
					SyncSequence++;
					OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;

					if (ClockSyncer.IsBroadAccepted())
					{
						UInt32ToArray(SyncClock.GetRollingMicros() + GetSendDuration(Linking::ClockSyncFineRequest::PAYLOAD_SIZE)
							, &OutPacket.Payload[Linking::ClockSyncFineRequest::PAYLOAD_ESTIMATE_INDEX]);
						if (SendPacket(OutPacket.Data, Linking::ClockSyncFineRequest::PAYLOAD_SIZE))
						{
							ClockSyncer.OnFineEstimateSent(micros());
						}
					}
					else
					{
						SyncClock.GetTimestamp(LinkTimestamp);
						LinkTimestamp.ShiftSubSeconds(GetSendDuration(Linking::ClockSyncBroadRequest::PAYLOAD_SIZE));
						UInt32ToArray(LinkTimestamp.Seconds, &OutPacket.Payload[Linking::ClockSyncBroadRequest::PAYLOAD_ESTIMATE_INDEX]);
						if (SendPacket(OutPacket.Data, Linking::ClockSyncBroadRequest::PAYLOAD_SIZE))
						{
							ClockSyncer.OnBroadEstimateSent(micros());
						}
					}
				}
				LOLA_RTOS_RESUME();
			}
		}

		Task::enableDelayed(0);
	}

	void OnServiceAuthenticating() final
	{
		if (AuthenticationReplyPending)
		{
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Linking::ClientChallengeReplyRequest::HEADER);
				Session.SignPartnerChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]);
				Session.CopyLocalChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);

				LOLA_RTOS_PAUSE();
				if (UnlinkedDuplexCanSend(Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE) &&
					PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE))
					{
						AuthenticationReplyPending = false;
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent ClientChallengeReplyRequest"));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
			Task::enableDelayed(0);
		}
		else
		{
			// Server will be the one initiating the authentication step.
			Task::enableDelayed(1);
		}
	}

	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint16_t rollingCounter, const uint8_t payloadSize)
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::SearchReply::HEADER:
			if (payloadSize == Unlinked::SearchReply::PAYLOAD_SIZE)
			{
				switch (LinkStage)
				{
				case LinkStageEnum::Sleeping:
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Woke up!"));
#endif
					UpdateLinkStage(LinkStageEnum::Searching);
				case LinkStageEnum::Searching:
					UpdateLinkStage(LinkStageEnum::Pairing);
					break;
				default:
#if defined(DEBUG_LOLA_LINK)
					this->Skipped(F("SearchReply"));
#endif
					return;
					break;
				}
				Task::enableDelayed(0);

#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Found a server!"));
#endif
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("SearchReply")); }
#endif
			break;
		case Unlinked::LinkingTimedSwitchOver::HEADER:
			if (payloadSize == Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE
				&& LinkStage == LinkStageEnum::Pairing
				&& Session.PairingTokenMatches(&payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]))
			{
				SetReceiveCounter(rollingCounter);	// Save last received counter, ready for switch for next stage.
				StateTransition.OnReceived(timestamp, &payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				SyncSequence = payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX];
				Task::enableDelayed(0);
				OnLinkSyncReceived(timestamp);
				UpdateLinkStage(LinkStageEnum::SwitchingToLinking);
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.print(F("Got LinkingTimedSwitchOver: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(timestamp));
				Serial.println(F("us remaining."));
#endif
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("LinkingTimedSwitchOver")); }
#endif
			break;
		default:
			break;
		}
	}

	void OnLinkingPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) final
	{
		switch (payload[Linking::ServerChallengeRequest::HEADER_INDEX])
		{
		case Linking::ServerChallengeRequest::HEADER:
			if (payloadSize == Linking::ServerChallengeRequest::PAYLOAD_SIZE
				&& LinkStage == LinkStageEnum::Authenticating)
			{
				Session.SetPartnerChallenge(&payload[Linking::ServerChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);
				AuthenticationReplyPending = true;
				OnLinkSyncReceived(timestamp);
				Task::enableDelayed(0);
				ResetUnlinkedPacketThrottle();
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Got ServerChallengeRequest"));
#endif
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("ServerChallengeRequest")); }
#endif
			break;
		case Linking::ServerChallengeReply::HEADER:
			if (payloadSize == Linking::ServerChallengeReply::PAYLOAD_SIZE
				&& LinkStage == LinkStageEnum::Authenticating
				&& Session.VerifyChallengeSignature(&payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]))
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Got ServerChallengeReply"));
#endif

				OnLinkSyncReceived(timestamp);
				Task::enableDelayed(0);
				ResetUnlinkedPacketThrottle();
				UpdateLinkStage(LinkStageEnum::ClockSyncing);
			}
#if defined(DEBUG_LOLA_LINK)
			else {
				this->Skipped(F("ServerChallengeReply"));
			}
#endif
			break;
		case Linking::ClockSyncBroadReply::HEADER:
			if (payloadSize == Linking::ClockSyncBroadReply::PAYLOAD_SIZE
				&& LinkStage == LinkStageEnum::ClockSyncing)
			{
				if (SyncSequence == payload[Linking::ClockSyncBroadReply::PAYLOAD_REQUEST_ID_INDEX]
					&& ClockSyncer.HasPendingReplyBroad()
					&& !ClockSyncer.IsBroadAccepted())
				{
					ClockSyncer.OnBroadReplyReceived(micros(), payload[Linking::ClockSyncBroadReply::PAYLOAD_ACCEPTED_INDEX]);

					// Adjust local seconds to match estimation error.
					SyncClock.ShiftSeconds(ArrayToInt32(&payload[Linking::ClockSyncBroadReply::PAYLOAD_ERROR_INDEX]));
				}
				else
				{
					ClockSyncer.Reset(micros());
#if defined(DEBUG_LOLA_LINK)
					Serial.println(F("ClockSyncer broad error, reset."));
#endif
				}
			}
#if defined(DEBUG_LOLA_LINK)
			else
			{
				this->Skipped(F("ClockSyncBroadReply."));
			}
#endif
			break;
		case Linking::ClockSyncFineReply::HEADER:
			if (payloadSize == Linking::ClockSyncFineReply::PAYLOAD_SIZE
				&& LinkStage == LinkStageEnum::ClockSyncing)
			{
				if (SyncSequence == payload[Linking::ClockSyncFineReply::PAYLOAD_REQUEST_ID_INDEX]
					&& ClockSyncer.HasPendingReplyFine()
					&& ClockSyncer.IsFineStarted())
				{
					ClockSyncer.OnFineReplyReceived(micros(), payload[Linking::ClockSyncFineReply::PAYLOAD_ACCEPTED_INDEX]);

					// Adjust local sub-seconds to match estimation error.
					SyncClock.ShiftSubSeconds(ArrayToInt32(&payload[Linking::ClockSyncFineReply::PAYLOAD_ERROR_INDEX]));

					if (!ClockAccepted && ClockSyncer.IsFineAccepted())
					{
						ClockAccepted = true;
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Clock Accepted, starting final step."));
#endif
						StateTransition.Clear();
						ResetUnlinkedPacketThrottle();
					}
				}
#if defined(DEBUG_LOLA_LINK)
				else
				{
					Serial.println(F("ClockSyncer fine error."));

				}
			}
			else
			{
				this->Skipped(F("ClockSyncFineReply."));
#endif
			}
			break;
		case Linking::LinkTimedSwitchOver::HEADER:
			if (payloadSize == Linking::LinkTimedSwitchOver::PAYLOAD_SIZE)
			{
				switch (LinkStage)
				{
				case LinkStageEnum::ClockSyncing:
					UpdateLinkStage(LinkStageEnum::SwitchingToLinked);
					break;
				case LinkStageEnum::SwitchingToLinked:
					break;
				default:
					this->Skipped(F("LinkSwitchOver2"));
					return;
					break;
				}

				StateTransition.OnReceived(timestamp, &payload[Linking::LinkTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				SyncSequence = payload[Linking::LinkTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX];
				Task::enableDelayed(0);

#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.print(F("Got LinkTimedSwitchOver: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(timestamp));
				Serial.println(F("us remaining."));
#endif
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("LinkSwitchOver")); }
#endif
			break;
		default:
			break;
		}
	}

	void OnPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
		if (port == LoLaLinkDefinition::LINK_PORT)
		{
			switch (payload[HeaderDefinition::HEADER_INDEX])
			{
			case Linked::ClockTuneReply::HEADER:
				if (payloadSize == Linked::ClockTuneReply::PAYLOAD_SIZE
					&& ClockTracker.WaitingForClockReply())
				{
					ClockTracker.OnReplyReceived(micros(), ArrayToInt32(&payload[Linked::ClockTuneReply::PAYLOAD_ERROR_INDEX]));
				}
#if defined(DEBUG_LOLA_LINK)
				else {
					this->Skipped(F("ClockTuneReply"));
				}
#endif
				break;
			default:
				BaseClass::OnPacketReceived(timestamp, payload, payloadSize, port);
				break;
			}
		}
	}

private:
	const uint32_t GetClockSyncRetryPeriod()
	{
		return Duplex->GetPeriod();
	}

	static const RequestPriority GetClockSyncPriority(const uint8_t clockQuality)
	{
		uint8_t measure = UINT8_MAX - clockQuality;
		if (measure >= (UINT8_MAX / 2))
		{
			measure = UINT8_MAX;
		}
		else
		{
			measure *= 2;
		}

		return GetProgressPriority<RequestPriority::IRREGULAR, RequestPriority::RESERVED_FOR_LINK>(measure);
	}

	void OnServiceSearching() final
	{
		if (UnlinkedPacketThrottle())
		{
			if (SearchChannelTryCount >= CHANNEL_SEARCH_TRY_COUNT)
			{
				// Switch channels after a few ms of failed attempt.
				// Has no effect if Channel Hop is permanent.
				SearchChannel++;
				SetAdvertisingChannel(SearchChannel);
				SearchChannelTryCount = 0;
			}

			LOLA_RTOS_PAUSE();
			if (PacketService.CanSendPacket())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Unlinked::SearchRequest::HEADER);
				if (SendPacket(OutPacket.Data, Unlinked::SearchRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Sent Search"));
#endif
				}
				SearchChannelTryCount++;
			}
			LOLA_RTOS_RESUME();
		}
		Task::enableDelayed(0);
	}

	void OnServiceSwitchingToLinking() final
	{
		if (StateTransition.HasTimedOut(micros()))
		{
			if (StateTransition.HasAcknowledge())
			{
				UpdateLinkStage(LinkStageEnum::Authenticating);
			}
			else
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("StateTransition timed out."));
				// No Ack before time out.
				UpdateLinkStage(LinkStageEnum::Searching);
#endif
			}
		}
		else if (StateTransition.IsSendRequested(micros()))
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.SetHeader(Unlinked::LinkingTimedSwitchOverAck::HEADER);
			OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;
			Session.GetPairingToken(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SESSION_TOKEN_INDEX]);

			LOLA_RTOS_PAUSE();
			if (PacketService.CanSendPacket())
			{
				if (SendPacket(OutPacket.Data, Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE))
				{
					StateTransition.OnSent();
				}
			}
			LOLA_RTOS_RESUME();
		}
		Task::enableDelayed(0);
	}

	void OnServiceSwitchingToLinked() final
	{
		if (StateTransition.HasTimedOut(micros()))
		{
			if (StateTransition.HasAcknowledge())
			{
				UpdateLinkStage(LinkStageEnum::Linked);
			}
			else
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("StateTransition timed out."));
#endif
				UpdateLinkStage(LinkStageEnum::Pairing);
			}
		}
		else if (StateTransition.IsSendRequested(micros()))
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.SetHeader(Linking::LinkTimedSwitchOverAck::HEADER);
			OutPacket.Payload[Linking::LinkTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;

			LOLA_RTOS_PAUSE();
			if (PacketService.CanSendPacket())
			{
				if (SendPacket(OutPacket.Data, Linking::LinkTimedSwitchOverAck::PAYLOAD_SIZE))
				{
					StateTransition.OnSent();
				}
			}
			LOLA_RTOS_RESUME();
		}
	}

protected:
	void OnLinkSyncReceived(const uint32_t receiveTimestamp)
	{
		LOLA_RTOS_PAUSE();
		LinkingDuplex.OnReceivedSync(SyncClock.GetRollingMonotonicMicros() - (micros() - receiveTimestamp));
		LOLA_RTOS_RESUME();
	}

	/// <summary>
	/// Pre-link half-duplex.
	/// Client follows last Server duplexed receive timestamp.
	/// 1/4 Slot for 2x Duplex duration.
	/// </summary>
	/// <param name="payloadSize"></param>
	/// <returns>True when packet can be sent.</returns>
	const bool UnlinkedDuplexCanSend(const uint8_t payloadSize)
	{
		const uint32_t timestamp = SyncClock.GetRollingMonotonicMicros();

		return LinkingDuplex.IsInRange(timestamp + GetSendDuration(payloadSize) + LinkingDuplex.GetFollowerOffset(), GetOnAirDuration(payloadSize));

	}
};
#endif
