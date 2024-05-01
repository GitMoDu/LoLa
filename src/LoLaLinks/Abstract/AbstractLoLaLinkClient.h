// AbstractLoLaLinkClient.h

#ifndef _ABSTRACT_LOLA_LINK_CLIENT_
#define _ABSTRACT_LOLA_LINK_CLIENT_

#include "..\Abstract\AbstractLoLaLink.h"
#include "..\..\Link\TimedStateTransition.h"
#include "..\..\Link\LinkClockTracker.h"
#include "..\..\Link\LinkClockSync.h"
#include "..\..\Link\PreLinkDuplex.h"

class AbstractLoLaLinkClient : public AbstractLoLaLink
{
private:
	using BaseClass = AbstractLoLaLink;

private:
	static constexpr uint32_t CLIENT_SLEEP_TIMEOUT_MILLIS = 30000;
	static constexpr uint8_t CHANNEL_SEARCH_TRY_COUNT = 3;

	static constexpr uint8_t CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS = 50;

	enum class WaitingStateEnum : uint8_t
	{
		Sleeping,
		SearchingLink,
		SessionCreation,
		SwitchingToLinking
	};

	enum class LinkingStateEnum : uint8_t
	{
		WaitingForAuthenticationRequest,
		AuthenticationReply,
		AuthenticationRequest,
		ClockSyncing,
		RequestingLinkStart,
		SwitchingToLinked
	};

protected:
	ClientTimedStateTransition StateTransition;

private:
	PreLinkSlaveDuplex LinkingDuplex;

	LinkClientClockTracker ClockTracker;
	LinkClientClockSync ClockSyncer;

	WaitingStateEnum WaitingState = WaitingStateEnum::Sleeping;

	LinkingStateEnum LinkingState = LinkingStateEnum::WaitingForAuthenticationRequest;

	uint8_t SyncSequence = 0;

	uint8_t SearchChannel = 0;
	uint8_t SearchChannelTryCount = 0;

protected:
	virtual void OnServiceSessionCreation() {}
	virtual void ResetSessionCreation() {}

public:
	AbstractLoLaLinkClient(Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, linkRegistry, encoder, transceiver, cycles, entropy, duplex, hop)
		, StateTransition(LoLaLinkDefinition::GetTransitionDuration(duplex->GetPeriod()))
		, LinkingDuplex(duplex->GetPeriod())
		, ClockTracker(duplex->GetPeriod())
		, ClockSyncer()
	{}

protected:
#if defined(DEBUG_LOLA)
	virtual void Owner() final
	{
		Serial.print(millis());
		Serial.print(F("\t[C] "));
	}
#endif

protected:
	const bool IsInSessionCreation()
	{
		return WaitingState == WaitingStateEnum::SessionCreation;
	}

protected:
	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint16_t rollingCounter, const uint8_t payloadSize)
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::SearchReply::HEADER:
			if (payloadSize == Unlinked::SearchReply::PAYLOAD_SIZE
				&& WaitingState == WaitingStateEnum::SearchingLink)
			{
				WaitingState = WaitingStateEnum::SessionCreation;
				ResetSessionCreation();
				ResetStageStartTime();
				Task::enable();
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
				SetReceiveCounter(rollingCounter);	// Save last received counter, ready for switch for next stage.
				StateTransition.OnReceived(timestamp, &payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				SyncSequence = payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX];
				Task::enable();
				OnLinkSyncReceived(timestamp);
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

	virtual void OnLinkingPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) final
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
				OnLinkSyncReceived(timestamp);
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("ServerChallengeRequest.")); }
#endif
			break;
		case Linking::ServerChallengeReply::HEADER:
			if (payloadSize == Linking::ServerChallengeReply::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::AuthenticationReply
				&& Encoder->VerifyChallengeSignature(&payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]))
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Link Access Control granted."));
#endif
				LinkingState = LinkingStateEnum::ClockSyncing;
				Task::enable();
				OnLinkSyncReceived(timestamp);
			}
#if defined(DEBUG_LOLA_LINK)
			else {
				this->Skipped(F("ServerChallengeReply"));
			}
#endif
			break;
		case Linking::ClockSyncBroadReply::HEADER:
			if (payloadSize == Linking::ClockSyncBroadReply::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::ClockSyncing)
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
				&& LinkingState == LinkingStateEnum::ClockSyncing)
			{
				if (SyncSequence == payload[Linking::ClockSyncFineReply::PAYLOAD_REQUEST_ID_INDEX]
					&& ClockSyncer.HasPendingReplyFine()
					&& ClockSyncer.IsFineStarted())
				{
					ClockSyncer.OnFineReplyReceived(micros(), payload[Linking::ClockSyncFineReply::PAYLOAD_ACCEPTED_INDEX]);

					// Adjust local sub-seconds to match estimation error.
					SyncClock.ShiftSubSeconds(ArrayToInt32(&payload[Linking::ClockSyncFineReply::PAYLOAD_ERROR_INDEX]));
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
				switch (LinkingState)
				{
				case LinkingStateEnum::RequestingLinkStart:
					LinkingState = LinkingStateEnum::SwitchingToLinked;
					break;
				case LinkingStateEnum::SwitchingToLinked:
					break;
				default:
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("LinkSwitchOver rejected."));
#endif
					return;
					break;
				}

				StateTransition.OnReceived(timestamp, &payload[Linking::LinkTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				SyncSequence = payload[Linking::LinkTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX];
				Task::enable();

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

	virtual void OnPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
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
			break;
		case LinkStageEnum::AwaitingLink:
			ResetSearch();
			break;
		case LinkStageEnum::Linking:
			ClockSyncer.Reset(micros());
			ClockTracker.Reset();
			StateTransition.Clear();
			Encoder->GenerateLocalChallenge(&RandomSource);
			LinkingState = LinkingStateEnum::WaitingForAuthenticationRequest;
			break;
		case LinkStageEnum::Linked:
#if defined(DEBUG_LOLA_LINK)
			Serial.print(GetLinkingElapsed() / 1000);
			Serial.println(F(" ms to Link."));
#endif
			ClockTracker.Reset(micros());
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
			if (GetStageElapsed() / ONE_MILLI_MICROS > CLIENT_SLEEP_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA_LINK)
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
			if ((GetStageElapsed() / ONE_MILLI_MICROS) > CLIENT_SLEEP_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("SessionCreation timed out. Going back to sleep."));
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
			if (GetStageElapsed() / ONE_MILLI_MICROS > CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA_LINK)
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
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Linking::ClientChallengeReplyRequest::HEADER);
				Encoder->SignPartnerChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]);
				Encoder->CopyLocalChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);

				LOLA_RTOS_PAUSE();
				if (UnlinkedDuplexCanSend(Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE) &&
					PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent ClientChallengeReplyRequest."));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
			Task::enable();
			break;
		case LinkingStateEnum::ClockSyncing:
			if (ClockSyncer.IsFineAccepted())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Clock Accepted, starting final step."));
#endif
				StateTransition.Clear();
				ResetUnlinkedPacketThrottle();
				LinkingState = LinkingStateEnum::RequestingLinkStart;
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
							SyncClock.GetTimestamp(LinkTimestamp);
							UInt32ToArray(LinkTimestamp.GetRollingMicros() + GetSendDuration(Linking::ClockSyncFineRequest::PAYLOAD_SIZE)
								, &OutPacket.Payload[Linking::ClockSyncFineRequest::PAYLOAD_ESTIMATE_INDEX]);
							if (SendPacket(OutPacket.Data, Linking::ClockSyncFineRequest::PAYLOAD_SIZE))
							{
								ClockSyncer.OnFineEstimateSent(micros());
							}
						}
						else
						{
							SyncClock.GetTimestamp(LinkTimestamp);
							UInt32ToArray(LinkTimestamp.Seconds, &OutPacket.Payload[Linking::ClockSyncFineRequest::PAYLOAD_ESTIMATE_INDEX]);
							if (SendPacket(OutPacket.Data, Linking::ClockSyncBroadRequest::PAYLOAD_SIZE))
							{
								ClockSyncer.OnBroadEstimateSent(micros());
							}
						}
					}
					LOLA_RTOS_RESUME();
				}
			}

			Task::enable();
			break;
		case LinkingStateEnum::RequestingLinkStart:
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
						Serial.println(F("Sent RequestingLinkStart."));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
			Task::enable();
			break;
		case LinkingStateEnum::SwitchingToLinked:
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
					UpdateLinkStage(LinkStageEnum::AwaitingLink);
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
			Task::enable();
			break;
		default:
			break;
		}
	}

	virtual void OnPreSend() final
	{
		if (OutPacket.GetPort() == LoLaLinkDefinition::LINK_PORT
			&& OutPacket.GetHeader() == Linked::ClockTuneRequest::HEADER)
		{
			SyncClock.GetTimestamp(LinkTimestamp);

			UInt32ToArray(LinkTimestamp.GetRollingMicros() + GetSendDuration(Linked::ClockTuneRequest::PAYLOAD_SIZE) + GetTimestampingDuration()
				, &OutPacket.Payload[Linked::ClockTuneRequest::PAYLOAD_ROLLING_INDEX]);
		}
	}

	virtual const uint8_t GetClockSyncQuality() final
	{
		return ClockTracker.GetQuality();
	}

	virtual const bool CheckForClockSyncUpdate() final
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

	void ResetSearch()
	{
		SearchChannel = RandomSource.GetRandomShort();
		SetAdvertisingChannel(SearchChannel);
		SearchChannelTryCount = 0;
		ResetUnlinkedPacketThrottle();
		WaitingState = WaitingStateEnum::SearchingLink;
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
						Serial.println(F("Sent Broadcast Search."));
#endif
					}
					SearchChannelTryCount++;
				}
				LOLA_RTOS_RESUME();
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
				UpdateLinkStage(LinkStageEnum::Linking);
			}
			else
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("StateTransition timed out."));
				// No Ack before time out.
				WaitingState = WaitingStateEnum::SearchingLink;
#endif
			}
		}
		else if (StateTransition.IsSendRequested(micros()))
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.SetHeader(Unlinked::LinkingTimedSwitchOverAck::HEADER);
			OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;
			Encoder->CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SESSION_TOKEN_INDEX]);

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
		Task::enable();
	}

protected:
	void OnLinkSyncReceived(const uint32_t receiveTimestamp)
	{
		LOLA_RTOS_PAUSE();
		const uint32_t elapsed = micros() - receiveTimestamp;
		SyncClock.GetTimestampMonotonic(LinkTimestamp);
		LOLA_RTOS_RESUME();
		LinkTimestamp.ShiftSubSeconds(-(int32_t)elapsed);
		LinkingDuplex.OnReceivedSync(LinkTimestamp.GetRollingMicros());
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
		SyncClock.GetTimestampMonotonic(LinkTimestamp);
		LinkTimestamp.ShiftSubSeconds((int32_t)GetSendDuration(payloadSize)
			+ (int32_t)LinkingDuplex.GetFollowerOffset());

		return LinkingDuplex.IsInRange(LinkTimestamp.GetRollingMicros(), GetOnAirDuration(payloadSize));
	}
};
#endif
