// AbstractLoLaLinkServer.h

#ifndef _ABSTRACT_LOLA_LINK_SERVER_
#define _ABSTRACT_LOLA_LINK_SERVER_

#include "..\Abstract\AbstractLoLaLink.h"
#include "..\..\Link\TimedStateTransition.h"
#include "..\..\Link\LinkClockSync.h"
#include "..\..\Link\LinkClockTracker.h"
#include "..\..\Link\PreLinkDuplex.h"

class AbstractLoLaLinkServer : public AbstractLoLaLink
{
private:
	using BaseClass = AbstractLoLaLink;

	static constexpr uint32_t SERVER_SLEEP_TIMEOUT_MILLIS = 30000;

	enum class WaitingStateEnum : uint8_t
	{
		Sleeping,
		SearchingLink,
		SessionCreation,
		SwitchingToLinking
	};

	enum class LinkingStateEnum : uint8_t
	{
		AuthenticationRequest,
		AuthenticationReply,
		ClockSyncing,
		SwitchingToLinked
	};

protected:
	ServerTimedStateTransition StateTransition;

private:
	LinkServerClockSync ClockSyncer{};
	LinkServerClockTracker ClockTracker{};

	PreLinkMasterDuplex LinkingDuplex;

	WaitingStateEnum WaitingState = WaitingStateEnum::Sleeping;

	LinkingStateEnum LinkingState = LinkingStateEnum::AuthenticationRequest;

	uint8_t SyncSequence = 0;

	bool SearchReplyPending = false;

protected:
	virtual void OnServiceSearchingLink() {}
	virtual void OnServiceSessionCreation() {}
	virtual void ResetSessionCreation() {}

public:
	AbstractLoLaLinkServer(Scheduler& scheduler,
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
	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint16_t rollingCounter, const uint8_t payloadSize)
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::SearchRequest::HEADER:
			if (payloadSize == Unlinked::SearchRequest::PAYLOAD_SIZE)
			{
				switch (WaitingState)
				{
				case WaitingStateEnum::Sleeping:
#if defined(DEBUG_LOLA_LINK)
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
#if defined(DEBUG_LOLA_LINK)
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
#if defined(DEBUG_LOLA_LINK)
					this->Skipped(F("LinkingTimedSwitchOverAck Bad SyncSequence."));
#endif
					return;
				}
				SetReceiveCounter(rollingCounter);	// Save last received counter, ready for switch for next stage.
				StateTransition.OnReceived();
				Task::enable();

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

	virtual void OnLinkingPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Linking::ClientChallengeReplyRequest::HEADER:
			if (LinkingState == LinkingStateEnum::AuthenticationRequest
				&& payloadSize == Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE
				&& Encoder->VerifyChallengeSignature(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]))
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Link Access Control granted."));
#endif

				Encoder->SetPartnerChallenge(&payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);
				LinkingState = LinkingStateEnum::AuthenticationReply;
				Task::enable();
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("ClientChallengeReplyRequest")); }
#endif
			break;
		case Linking::ClockSyncBroadRequest::HEADER:
			if (payloadSize == Linking::ClockSyncBroadRequest::PAYLOAD_SIZE)
			{
				switch (LinkingState)
				{
				case LinkingStateEnum::AuthenticationReply:
					LinkingState = LinkingStateEnum::ClockSyncing;
				case LinkingStateEnum::ClockSyncing:
					if (ClockSyncer.IsFineStarted())
					{
#if defined(DEBUG_LOLA_LINK)
						this->Skipped(F("ClockSyncBroadRequest"));
#endif
						return;
					}
					break;
				default:
					return;
					break;
				}

				LOLA_RTOS_PAUSE();
				SyncClock.GetTimestamp(LinkTimestamp);
				LinkTimestamp.ShiftSubSeconds(-(int32_t)(micros() - timestamp));
				LOLA_RTOS_RESUME();

				SyncSequence = payload[Linking::ClockSyncBroadRequest::PAYLOAD_REQUEST_ID_INDEX];
				ClockSyncer.OnBroadEstimateReceived(LinkTimestamp.Seconds,
					ArrayToUInt32(&payload[Linking::ClockSyncBroadRequest::PAYLOAD_ESTIMATE_INDEX]));

				Task::enable();
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("ClockSyncBroadRequest")); }
#endif
			break;
		case Linking::ClockSyncFineRequest::HEADER:
			if (payloadSize == Linking::ClockSyncFineRequest::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::ClockSyncing
				&& ClockSyncer.IsBroadAccepted())
			{
				SyncSequence = payload[Linking::ClockSyncFineRequest::PAYLOAD_REQUEST_ID_INDEX];

				LOLA_RTOS_PAUSE();
				SyncClock.GetTimestamp(LinkTimestamp);
				ClockSyncer.OnFineEstimateReceived(LinkTimestamp.GetRollingMicros() - ((int32_t)(micros() - timestamp)),
					ArrayToUInt32(&payload[Linking::ClockSyncFineRequest::PAYLOAD_ESTIMATE_INDEX]));
				LOLA_RTOS_RESUME();
				Task::enable();
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("ClockSyncBroadRequest")); }
#endif
			break;
		case Linking::StartLinkRequest::HEADER:
			if (payloadSize == Linking::StartLinkRequest::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::ClockSyncing
				&& ClockSyncer.IsFineAccepted())
			{

#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Got Link Start Request."));
#endif
				LinkingState = LinkingStateEnum::SwitchingToLinked;
				StateTransition.OnStart(micros());
				ResetUnlinkedPacketThrottle();
				Task::enable();
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("StartLinkRequest")); }
#endif
			break;
		case Linking::LinkTimedSwitchOverAck::HEADER:
			if (payloadSize == Linking::LinkTimedSwitchOverAck::PAYLOAD_SIZE
				&& LinkingState == LinkingStateEnum::SwitchingToLinked)
			{
				if (SyncSequence != payload[Linking::LinkTimedSwitchOverAck::PAYLOAD_REQUEST_ID_INDEX])
				{
#if defined(DEBUG_LOLA_LINK)
					this->Skipped(F("LinkTimedSwitchOverAck Bad SyncSequence."));
#endif
					return;
				}

				StateTransition.OnReceived();
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.print(F("StateTransition Server got Ack: "));
				Serial.print(StateTransition.GetDurationUntilTimeOut(timestamp));
				Serial.println(F("us remaining."));
#endif
				Task::enable();
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("LinkTimedSwitchOverAck")); }
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
			case Linked::ClockTuneRequest::HEADER:
				if (payloadSize == Linked::ClockTuneRequest::PAYLOAD_SIZE
					&& !ClockTracker.HasReplyPending())
				{
					LOLA_RTOS_PAUSE();
					SyncClock.GetTimestamp(LinkTimestamp);
					ClockTracker.OnLinkedEstimateReceived(LinkTimestamp.GetRollingMicros() - ((int32_t)(micros() - timestamp))
						, ArrayToUInt32(&payload[Linked::ClockTuneRequest::PAYLOAD_ROLLING_INDEX]));
					LOLA_RTOS_RESUME();
					Task::enable();
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
			SetAdvertisingChannel(RandomSource.GetRandomShort());
			NewSession();
			WaitingState = WaitingStateEnum::Sleeping;
			break;
		case LinkStageEnum::Linking:
			Encoder->GenerateLocalChallenge(&RandomSource);
			LinkingState = LinkingStateEnum::AuthenticationRequest;
			ClockSyncer.Reset();
			ClockTracker.Reset();
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
			if (GetStageElapsed() / ONE_MILLI_MICROS > SERVER_SLEEP_TIMEOUT_MILLIS)
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
				OnServiceSearchingLinkInternal();
			}
			break;
		case WaitingStateEnum::SessionCreation:
			if (GetStageElapsed() / ONE_MILLI_MICROS > SERVER_SLEEP_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("SessionCreation timed out. Going to sleep."));
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
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Linking::ServerChallengeRequest::HEADER);
				Encoder->CopyLocalChallengeTo(&OutPacket.Payload[Linking::ServerChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);
				LOLA_RTOS_PAUSE();
				if (UnlinkedDuplexCanSend(Linking::ServerChallengeRequest::PAYLOAD_SIZE) &&
					PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Linking::ServerChallengeRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent ServerChallengeRequest."));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
			Task::enable();
			break;
		case LinkingStateEnum::AuthenticationReply:
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Linking::ServerChallengeReply::HEADER);
				Encoder->SignPartnerChallengeTo(&OutPacket.Payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]);

				LOLA_RTOS_PAUSE();
				if (UnlinkedDuplexCanSend(Linking::ServerChallengeReply::PAYLOAD_SIZE) &&
					PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Linking::ServerChallengeReply::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent ServerChallengeReply."));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
			Task::enable();
			break;
		case LinkingStateEnum::ClockSyncing:
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
					Serial.println(F("StateTransition failed."));
#endif
					UpdateLinkStage(LinkStageEnum::AwaitingLink);
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
			Task::enable();
			break;
		default:
			break;
		}
	}

	virtual const uint8_t GetClockSyncQuality() final
	{
		return ClockTracker.GetQuality();
	}

	virtual const bool CheckForClockSyncUpdate() final
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
		Encoder->SetRandomSessionId(&RandomSource);
		SyncClock.ShiftSeconds(RandomSource.GetRandomLong());
		SyncClock.ShiftSubSeconds(RandomSource.GetRandomLong());
	}

	void OnServiceSearchingLinkInternal()
	{
		switch (WaitingState)
		{
		case WaitingStateEnum::Sleeping:
			Task::disable();
#if defined(DEBUG_LOLA_LINK)
			this->Owner();
			Serial.println(F("Search time out, going to sleep."));
#endif
			break;
		case WaitingStateEnum::SearchingLink:
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
				UpdateLinkStage(LinkStageEnum::Linking);
			}
			else
			{
				// No Ack before time out.
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("StateTransition time out."));
#endif
				NewSession();
				WaitingState = WaitingStateEnum::SearchingLink;
				SearchReplyPending = false;
				ResetSessionCreation();
			}
		}
		else if (StateTransition.IsSendRequested(micros())
			&& UnlinkedPacketThrottle())
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.SetHeader(Unlinked::LinkingTimedSwitchOver::HEADER);
			Encoder->CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]);

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
	//const bool UnlinkedCanSendPacket(const uint8_t payloadSize)
	const bool UnlinkedDuplexCanSend(const uint8_t payloadSize)
	{
		SyncClock.GetTimestamp(LinkTimestamp);

		return LinkingDuplex.IsInRange(LinkTimestamp.GetRollingMicros() + GetSendDuration(payloadSize), GetOnAirDuration(payloadSize));
	}
};
#endif