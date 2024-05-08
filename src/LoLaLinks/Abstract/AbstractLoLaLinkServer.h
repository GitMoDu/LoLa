// AbstractLoLaLinkServer.h

#ifndef _ABSTRACT_LOLA_LINK_SERVER_
#define _ABSTRACT_LOLA_LINK_SERVER_

#include "AbstractLoLaLink.h"
#include "../../Link/TimedStateTransition.h"
#include "../../Link/LinkClockSync.h"
#include "../../Link/LinkClockTracker.h"
#include "../../Link/PreLinkDuplex.h"

class AbstractLoLaLinkServer : public AbstractLoLaLink
{
private:
	using BaseClass = AbstractLoLaLink;

private:
	static constexpr uint32_t SEARCH_CHANNEL_LINGER_DURATION = 250 * ONE_MILLI_MICROS;

protected:
	ServerTimedStateTransition StateTransition;

private:
	LinkServerClockSync ClockSyncer{};
	LinkServerClockTracker ClockTracker{};

	PreLinkMasterDuplex LinkingDuplex;

	uint32_t ChannelSearchStart = 0;
	uint8_t SyncSequence = 0;
	bool ClientAuthenticated = false;
	bool SearchReplyPending = false;

public:
	AbstractLoLaLinkServer(Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, linkRegistry, transceiver, cycles, entropy, duplex, hop)
		, StateTransition(LoLaLinkDefinition::GetTransitionDuration(duplex->GetPeriod()))
		, LinkingDuplex(duplex->GetPeriod())
	{}

protected:
#if defined(DEBUG_LOLA)
	void Owner() final
	{
		Serial.print(millis());
		Serial.print(F("\t[S] "));
	}
#endif

protected:
	const uint8_t GetClockQuality() final
	{
		return ClockTracker.GetQuality();
	}

	/// <summary>
	/// Pre-link half-duplex.
	/// Server follows accurate but random SyncClock.
	/// 1/4 Slot for 2x Duplex duration.
	/// </summary>
	/// <param name="payloadSize"></param>
	/// <returns>True when packet can be sent.</returns>
	//const bool UnlinkedCanSendPacket(const uint8_t payloadSize)
	const bool UnlinkedDuplexCanSend(const uint8_t payloadSize)
	{
		return LinkingDuplex.IsInRange(SyncClock.GetRollingMicros() + GetSendDuration(payloadSize), GetOnAirDuration(payloadSize));
	}

	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::Booting:
			SetAdvertisingChannel(RandomSource.GetRandomShort());
			break;
		case LinkStageEnum::Sleeping:
			break;
		case LinkStageEnum::Searching:
			ChannelSearchStart = micros();
			NewSession();
			SearchReplyPending = false;
			ResetUnlinkedPacketThrottle();
			break;
		case LinkStageEnum::Pairing:
			NewSession();
			break;
		case LinkStageEnum::SwitchingToLinking:
			ClientAuthenticated = false;
		case LinkStageEnum::SwitchingToLinked:
			StateTransition.OnStart(micros());
			ResetUnlinkedPacketThrottle();
			Task::enableDelayed(0);
			break;
		case LinkStageEnum::Authenticating:
			Session.GenerateLocalChallenge(&RandomSource);
			ClockSyncer.Reset();
			ClockTracker.Reset();
			break;
		case LinkStageEnum::ClockSyncing:
			break;
		case LinkStageEnum::Linked:
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
		//TODO: Set Transceiver RxSleep.
		Task::disable();
	}

	void OnServiceSearching() final
	{
		if (micros() - ChannelSearchStart > SEARCH_CHANNEL_LINGER_DURATION)
		{
			ChannelSearchStart = micros();
			SetAdvertisingChannel(RandomSource.GetRandomShort());
		}

		if (SearchReplyPending)
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.SetHeader(Unlinked::SearchReply::HEADER);

			LOLA_RTOS_PAUSE();
			if (PacketService.CanSendPacket())
			{
				if (SendPacket(OutPacket.Data, Unlinked::SearchReply::PAYLOAD_SIZE))
				{
					SearchReplyPending = false;
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Sent Search Reply."));
#endif
				}
			}
			LOLA_RTOS_RESUME();
			Task::enableDelayed(0);
		}
		else
		{
			Task::enableDelayed(100);
		}
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
				// No Ack before time out.
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("StateTransition time out."));
#endif
				UpdateLinkStage(LinkStageEnum::Searching);
			}
		}
		else if (StateTransition.IsSendRequested(micros())
			&& UnlinkedPacketThrottle())
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.SetHeader(Unlinked::LinkingTimedSwitchOver::HEADER);
			Session.GetPairingToken(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]);

			LOLA_RTOS_PAUSE();
			if (UnlinkedDuplexCanSend(Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE) &&
				PacketService.CanSendPacket())
			{
				SyncSequence++;
				OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;

				const uint32_t timestamp = micros();
				StateTransition.CopyDurationUntilTimeOutTo(timestamp + GetSendDuration(Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE), &OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				if (SendPacket(OutPacket.Data, Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.print(F("Sent LinkingTimedSwitchOver, "));
					Serial.print(StateTransition.GetDurationUntilTimeOut(timestamp + GetSendDuration(Linking::LinkTimedSwitchOver::PAYLOAD_SIZE)));
					Serial.println(F("us remaining."));
#endif
				}
			}
			LOLA_RTOS_RESUME();
		}
		Task::enableDelayed(0);
	}

	void OnServiceAuthenticating() final
	{
		if (ClientAuthenticated)
		{
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Linking::ServerChallengeReply::HEADER);
				Session.SignPartnerChallengeTo(&OutPacket.Payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]);

				LOLA_RTOS_PAUSE();
				if (UnlinkedDuplexCanSend(Linking::ServerChallengeReply::PAYLOAD_SIZE) &&
					PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Linking::ServerChallengeReply::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent ServerChallengeReply"));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
		}
		else
		{
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Linking::ServerChallengeRequest::HEADER);
				Session.CopyLocalChallengeTo(&OutPacket.Payload[Linking::ServerChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);
				LOLA_RTOS_PAUSE();
				if (UnlinkedDuplexCanSend(Linking::ServerChallengeRequest::PAYLOAD_SIZE) &&
					PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Linking::ServerChallengeRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent ServerChallengeRequest"));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
		}
		Task::enableDelayed(0);
	}

	void OnServiceClockSyncing() final
	{
		if (ClockSyncer.HasReplyPending())
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;
			Int32ToArray(ClockSyncer.GetErrorReply(), &OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_ERROR_INDEX]);
			if (ClockSyncer.IsPendingReplyFine())
			{
				OutPacket.SetHeader(Linking::ClockSyncFineReply::HEADER);
				OutPacket.Payload[Linking::ClockSyncFineReply::PAYLOAD_ACCEPTED_INDEX] = ClockSyncer.IsFineAccepted() * UINT8_MAX;
			}
			else
			{
				OutPacket.SetHeader(Linking::ClockSyncBroadReply::HEADER);
				OutPacket.Payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] = ClockSyncer.IsBroadAccepted() * UINT8_MAX;
			}

			LOLA_RTOS_PAUSE();
			if (PacketService.CanSendPacket()
				&& SendPacket(OutPacket.Data, Linking::ClockSyncReply::PAYLOAD_SIZE))
			{
				ClockSyncer.OnReplySent();
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
				Serial.println(F("StateTransition failed."));
#endif
				UpdateLinkStage(LinkStageEnum::Pairing);
			}
		}
		else if (StateTransition.IsSendRequested(micros())
			&& UnlinkedPacketThrottle())
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.SetHeader(Linking::LinkTimedSwitchOver::HEADER);

			LOLA_RTOS_PAUSE();
			if (UnlinkedDuplexCanSend(Linking::LinkTimedSwitchOver::PAYLOAD_SIZE) &&
				PacketService.CanSendPacket())
			{
				SyncSequence++;
				OutPacket.Payload[Linking::LinkTimedSwitchOver::PAYLOAD_REQUEST_ID_INDEX] = SyncSequence;

				const uint32_t timestamp = micros();
				StateTransition.CopyDurationUntilTimeOutTo(timestamp + GetSendDuration(Linking::LinkTimedSwitchOver::PAYLOAD_SIZE), &OutPacket.Payload[Linking::LinkTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				if (SendPacket(OutPacket.Data, Linking::LinkTimedSwitchOver::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.print(F("Sent LinkTimedSwitchOver, "));
					Serial.print(StateTransition.GetDurationUntilTimeOut(timestamp + GetSendDuration(Linking::LinkTimedSwitchOver::PAYLOAD_SIZE)));
					Serial.println(F("us remaining."));
#endif
				}
			}
			LOLA_RTOS_RESUME();
		}
		Task::enableDelayed(0);
	}

	const bool CheckForClockTuneUpdate() final
	{
		if (ClockTracker.HasReplyPending())
		{
			if (CanRequestSend())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Linked::ClockTuneReply::HEADER);
				Int32ToArray(ClockTracker.GetLinkedReplyError(), &OutPacket.Payload[Linked::ClockTuneReply::PAYLOAD_ERROR_INDEX]);

				if (RequestSendPacket(Linked::ClockTuneReply::PAYLOAD_SIZE, GetClockSyncPriority(ClockTracker.GetQuality())))
				{
					ClockTracker.OnReplySent();
				}
			}

			return true;
		}

		return false;
	}

	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint16_t rollingCounter, const uint8_t payloadSize)
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::SearchRequest::HEADER:
			if (payloadSize == Unlinked::SearchRequest::PAYLOAD_SIZE)
			{
				if (LinkStage == LinkStageEnum::Sleeping)
				{
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Wake up"));
#endif
					UpdateLinkStage(LinkStageEnum::Searching);
				}
				Task::enableDelayed(0);
				SearchReplyPending = true;
			}
#if defined(DEBUG_LOLA_LINK)
			else {
				this->Skipped(F("SearchRequest"));
			}
#endif
			break;
		case Unlinked::LinkingTimedSwitchOverAck::HEADER:
			if (payloadSize == Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE
				&& LinkStage == LinkStageEnum::SwitchingToLinking
				&& Session.PairingTokenMatches(&payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SESSION_TOKEN_INDEX])
				&& SyncSequence == payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX])
			{
				SetReceiveCounter(rollingCounter);	// Save last received counter, ready for switch for next stage.
				StateTransition.OnReceived();
				Task::enableDelayed(0);

#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.print(F("StateTransition Server got Ack: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(timestamp));
				Serial.println(F("us remaining."));
#endif				
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("LinkingTimedSwitchOverAck")); }
#endif
			break;
		default:
			break;
		}
	}

	void OnLinkingPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Linking::ClientChallengeReplyRequest::HEADER:
			if (LinkStage == LinkStageEnum::Authenticating
				&& payloadSize == Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE
				&& Session.VerifyChallengeSignature(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]))
			{
				ClientAuthenticated = true;
				Session.SetPartnerChallenge(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);
				Task::enableDelayed(0);
				ResetUnlinkedPacketThrottle();
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Link Access Control granted."));
#endif
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("ClientChallengeReplyRequest")); }
#endif
			break;
		case Linking::ClockSyncBroadRequest::HEADER:
			if (payloadSize == Linking::ClockSyncBroadRequest::PAYLOAD_SIZE
				&& ClientAuthenticated)
			{
				switch (LinkStage)
				{
				case LinkStageEnum::Authenticating:
					UpdateLinkStage(LinkStageEnum::ClockSyncing);
					break;
				case LinkStageEnum::ClockSyncing:
					if (ClockSyncer.IsFineStarted())
					{
#if defined(DEBUG_LOLA_LINK)
						this->Skipped(F("ClockSyncBroadRequest2"));
#endif
						return;
					}
					break;
				default:
#if defined(DEBUG_LOLA_LINK)
					this->Skipped(F("ClockSyncBroadRequest3"));
#endif
					return;
					break;
				}

				SyncSequence = payload[Linking::ClockSyncBroadRequest::PAYLOAD_REQUEST_ID_INDEX];
				ResetUnlinkedPacketThrottle();

				LOLA_RTOS_PAUSE();
				SyncClock.GetTimestamp(LinkTimestamp);
				LinkTimestamp.ShiftSubSeconds(-(int32_t)(micros() - timestamp));
				LOLA_RTOS_RESUME();
				ClockSyncer.OnBroadEstimateReceived(LinkTimestamp.Seconds,
					ArrayToUInt32(&payload[Linking::ClockSyncBroadRequest::PAYLOAD_ESTIMATE_INDEX]));
				Task::enableDelayed(0);

			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("ClockSyncBroadRequest")); }
#endif
			break;
		case Linking::ClockSyncFineRequest::HEADER:
			if (payloadSize == Linking::ClockSyncFineRequest::PAYLOAD_SIZE
				&& LinkStage == LinkStageEnum::ClockSyncing
				&& ClockSyncer.IsBroadAccepted())
			{
				SyncSequence = payload[Linking::ClockSyncFineRequest::PAYLOAD_REQUEST_ID_INDEX];
				ResetUnlinkedPacketThrottle();

				LOLA_RTOS_PAUSE();
				ClockSyncer.OnFineEstimateReceived(SyncClock.GetRollingMicros() - (micros() - timestamp),
					ArrayToUInt32(&payload[Linking::ClockSyncFineRequest::PAYLOAD_ESTIMATE_INDEX]));
				LOLA_RTOS_RESUME();
				Task::enableDelayed(0);
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("ClockSyncBroadRequest")); }
#endif
			break;
		case Linking::StartLinkRequest::HEADER:
			if (payloadSize == Linking::StartLinkRequest::PAYLOAD_SIZE
				&& LinkStage == LinkStageEnum::ClockSyncing
				&& ClockSyncer.IsFineAccepted())
			{
				UpdateLinkStage(LinkStageEnum::SwitchingToLinked);
				StateTransition.OnStart(micros());
				ResetUnlinkedPacketThrottle();
				Task::enableDelayed(0);
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Got Link Start Request"));
#endif
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("StartLinkRequest")); }
#endif
			break;
		case Linking::LinkTimedSwitchOverAck::HEADER:
			if (payloadSize == Linking::LinkTimedSwitchOverAck::PAYLOAD_SIZE
				&& SyncSequence == payload[Linking::LinkTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX])
			{
				switch (LinkStage)
				{
				case LinkStageEnum::ClockSyncing:
					UpdateLinkStage(LinkStageEnum::SwitchingToLinked);
					break;
				case LinkStageEnum::SwitchingToLinked:
					break;
				default:
#if defined(DEBUG_LOLA_LINK)
					this->Skipped(F("LinkSwitchOver2"));
#endif
					return;
					break;
				}
				StateTransition.OnReceived();
				Task::enableDelayed(0);
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.print(F("StateTransition Server got Ack: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(timestamp));
				Serial.println(F("us remaining."));
#endif
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("LinkTimedSwitchOverAck")); }
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
			case Linked::ClockTuneRequest::HEADER:
				if (payloadSize == Linked::ClockTuneRequest::PAYLOAD_SIZE
					&& !ClockTracker.HasReplyPending())
				{
					LOLA_RTOS_PAUSE();
					ClockTracker.OnLinkedEstimateReceived(SyncClock.GetRollingMicros() - (int16_t)(micros() - timestamp)
						, ArrayToUInt32(&payload[Linked::ClockTuneRequest::PAYLOAD_ROLLING_INDEX]));
					LOLA_RTOS_RESUME();
					Task::enableDelayed(0);
				}
#if defined(DEBUG_LOLA_LINK)
				else { this->Skipped(F("ClockTuneRequest")); }
#endif
				break;
			default:
				BaseClass::OnPacketReceived(timestamp, payload, payloadSize, port);
				break;
			}
		}
#if defined(DEBUG_LOLA_LINK)
		else { this->Skipped(F("Unknown")); }
#endif
	}

private:
	static const RequestPriority GetClockSyncPriority(const uint8_t clockQuality)
	{
		uint8_t measure = UINT8_MAX - clockQuality;
		if (measure >= (UINT8_MAX / 3))
		{
			measure = UINT8_MAX;
		}
		else
		{
			measure *= 3;
		}

		return GetProgressPriority<RequestPriority::REGULAR, RequestPriority::RESERVED_FOR_LINK>(measure);
	}

	void NewSession()
	{
		Session.SetRandomSessionId(&RandomSource);
		SyncClock.ShiftSeconds(RandomSource.GetRandomLong());
		SyncClock.ShiftSubSeconds(RandomSource.GetRandomLong());
	}
};
#endif