// LoLaLinkHostService.h

#ifndef _LOLA_LINK_HOST_SERVICE_h
#define _LOLA_LINK_HOST_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

#include <Services\Link\LoLaLinkLatencyMeter.h>


class LoLaLinkHostService : public LoLaLinkService
{
private:
	enum AwaitingLinkEnum : uint8_t
	{
		BroadcastingOpenSession = 0,
		LinkRequested = 1,
		LinkingSwitchOver = 2
	};

	LinkHostClockSyncer ClockSyncer;
	ClockSyncResponseTransaction HostClockSyncTransaction;

	ChallengeRequestTransaction HostChallengeTransaction;

	HostInfoSyncTransaction HostInfoTransaction;


	//Latency measurement.
	LoLaLinkLatencyMeter<LOLA_LINK_SERVICE_UNLINK_MAX_LATENCY_SAMPLES> LatencyMeter;

	//Session timing.
	uint32_t SessionLastStarted = ILOLA_INVALID_MILLIS;

private:
	void NewSession()
	{
		ClearSession();
		LinkInfo->SetSessionId((uint8_t)random(1, (UINT8_MAX - 1)));
		SessionLastStarted = millis();
	}

public:
	LoLaLinkHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		ClockSyncerPointer = &ClockSyncer;
		ClockSyncTransaction = &HostClockSyncTransaction;
		ChallengeTransaction = &HostChallengeTransaction;
		InfoTransaction = &HostInfoTransaction;
#ifdef USE_TIME_SLOT
		loLa->SetDuplexSlot(false);
#endif
	}
protected:

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Host"));
	}
#endif // DEBUG_LOLA

	//Host version, PartnerMAC is the Remote's MAC.
	void SetBaseSeed()
	{
		CryptoSeed.SetBaseSeed(LinkInfo->GetLocalMAC(), LinkInfo->GetPartnerMAC(), LOLA_LINK_INFO_MAC_LENGTH, LinkInfo->GetSessionId());
	}

	void OnLinkDiscoveryReceived()
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingSleeping)
		{
			SetNextRunASAP();
		}
	}

	void OnLinkRequestReceived(const uint8_t sessionId, uint8_t* remoteMAC)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (LinkingState == AwaitingLinkEnum::BroadcastingOpenSession &&
				LinkInfo->HasSessionId() &&
				LinkInfo->GetSessionId() == sessionId &&
				!LinkInfo->HasPartnerMAC())
			{
				//Here is where we have the first choice to reject this remote.
				//TODO: MAC Filtering?
				LinkInfo->SetPartnerMAC(remoteMAC);
				SetLinkingState(AwaitingLinkEnum::LinkRequested);
			}
			break;
		default:
			break;
		}
	}

	void OnLinkRequestReadyReceived(const uint8_t sessionId, uint8_t* remoteMAC)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			LinkingState == AwaitingLinkEnum::LinkRequested &&
			LinkInfo->HasSessionId() &&
			LinkInfo->GetSessionId() == sessionId &&
			MacManager.Match(LinkInfo->GetPartnerMAC(), remoteMAC))
		{
			SetLinkingState(AwaitingLinkEnum::LinkingSwitchOver);
		}
	}

	void OnLinkPacketAckReceived(const uint8_t requestId)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (LinkingState == AwaitingLinkEnum::LinkingSwitchOver &&
				LinkInfo->GetSessionId() == requestId &&
				LinkInfo->HasPartnerMAC())
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linking);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			switch (LinkingState)
			{
			case LinkingStagesEnum::ClockSyncSwitchOver:
				if (requestId == LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER)
				{
					SetLinkingState(LinkingStagesEnum::ChallengeStage);
				}
				break;
			case LinkingStagesEnum::ChallengeSwitchOver:
				if (requestId == ChallengeTransaction->GetTransactionId())
				{
					SetLinkingState(LinkingStagesEnum::InfoSyncStage);
				}
				break;
			case LinkingStagesEnum::InfoSyncStage:
				HostInfoTransaction.OnRequestAckReceived(requestId);
				SetNextRunASAP();
				break;
			case LinkingStagesEnum::LinkProtocolSwitchOver:
				if (LinkInfo->GetSessionId() == requestId)
				{
					SetLinkingState(LinkingStagesEnum::AllConnectingStagesDone);
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}

	void OnAwaitingConnection()
	{
		switch (LinkingState)
		{
		case AwaitingLinkEnum::BroadcastingOpenSession:
			if (SessionLastStarted == ILOLA_INVALID_MILLIS || millis() - SessionLastStarted > LOLA_LINK_SERVICE_UNLINK_SESSION_LIFETIME)
			{
				NewSession();
			}

			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_BROADCAST_PERIOD)
			{
				PreparePacketBroadcast();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::LinkRequested:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkRequestAccepted();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::LinkingSwitchOver:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				//Send the switch-over packet with Ack.
				PrepareLinkConnectingSwitchOver();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		default:
			SetLinkingState(0);
			SetNextRunASAP();
			break;
		}
	}

	///Clock Sync.
	void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ClockSyncStage)
		{
			HostClockSyncTransaction.SetResult(requestId,
				(int32_t)(ClockSyncer.GetMillisSynced(GetLoLa()->GetLastValidReceivedMillis()) - estimatedMillis));
			SetNextRunASAP();
		}
	}

	void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis)
	{
		if (LinkInfo->HasLink())
		{
			HostClockSyncTransaction.SetResult(requestId,
				(int32_t)(ClockSyncer.GetMillisSynced(GetLoLa()->GetLastValidReceivedMillis()) - estimatedMillis));
			SetNextRunASAP();
		}
	}

	void OnKeepingConnected()
	{
		if (HostClockSyncTransaction.IsResultReady())
		{
			if (HostClockSyncTransaction.GetResult() == 0)
			{
				ClockSyncer.StampSynced();
			}
#ifdef DEBUG_LOLA
			else
			{
				LinkInfo->StampClockSyncAdjustment();
			}
#endif

			ClockSyncer.OnEstimationReceived(HostClockSyncTransaction.GetResult());

			PrepareClockSyncTuneResponse(HostClockSyncTransaction.GetId(), ClockSyncer.GetLastError());
			HostClockSyncTransaction.Reset();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
		}
	}

	void OnClockSync()
	{
		if (ClockSyncer.IsSynced())
		{
			SetNextRunASAP();
		}
		else if (HostClockSyncTransaction.IsResultReady())
		{
			ClockSyncer.OnEstimationReceived(HostClockSyncTransaction.GetResult());

			PrepareClockSyncResponse(HostClockSyncTransaction.GetId(), ClockSyncer.GetLastError());
			HostClockSyncTransaction.Reset();
			RequestSendPacket(true);
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		}
	}

	void OnClockSyncSwitchOver()
	{
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
		{
			PrepareClockSyncSwitchOver();
			RequestSendPacket(true);
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		}
	}
	///

	///Challenge. Possibly CPU intensive task.
	void OnChallenging()
	{
		if (HostChallengeTransaction.IsStale())
		{
			HostChallengeTransaction.NewRequest();
		}

		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
		{
			PrepareChallengeRequest();
			RequestSendPacket(true);
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		}
	}

	void OnChallengeResponseReceived(const uint8_t requestId, const uint32_t token)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ChallengeStage)
		{
			HostChallengeTransaction.OnReply(requestId, token);
			SetNextRunASAP();
		}
	}

	void OnChallengeSwitchOver()
	{
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
		{
			PrepareChallengeSwitchOver();
			RequestSendPacket(true);
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		}
	}
	///

	///Info Sync
	void OnInfoSync()
	{
		switch (HostInfoTransaction.GetStage())
		{
		case InfoSyncTransaction::StageEnum::StageStart:
			//First move is done by host.
			//If we don't have enough latency samples, we make more.
			if (LatencyMeter.GetSampleCount() >= LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES)
			{
				LinkInfo->SetRTT(LatencyMeter.GetAverageLatency());
				HostInfoTransaction.Advance();
				SetNextRunASAP();
			}
			else if (GetElapsedLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareChallengeSwitchOver();
				RequestSendPacket(true);
				//Repeat last update packet.
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case InfoSyncTransaction::StageEnum::StageHostRTT:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkInfoSyncUpdate(InfoSyncTransaction::ContentIdEnum::ContentHostRTT, LatencyMeter.GetAverageLatency());
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case InfoSyncTransaction::StageEnum::StageHostRSSI:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkInfoSyncUpdate(InfoSyncTransaction::ContentIdEnum::ContentHostRSSI, LinkInfo->GetRSSINormalized());
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case InfoSyncTransaction::StageEnum::StageRemoteRSSI:
			if (LinkInfo->HasPartnerRSSI() && GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkInfoSyncAdvanceRequest(InfoSyncTransaction::ContentIdEnum::ContentRemoteRSSI);
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case InfoSyncTransaction::StageEnum::StagesDone:
			SetNextRunASAP();
			break;
		default:
			HostInfoTransaction.Clear();
			break;
		}
	}

	void OnLinkInfoSyncUpdateReceived(const uint8_t contentId, const uint32_t content)
	{
		if (HostInfoTransaction.OnUpdateReceived(contentId))
		{
			switch (HostInfoTransaction.GetStage())
			{
			case InfoSyncTransaction::StageEnum::StageRemoteRSSI:
				if (contentId == InfoSyncTransaction::ContentIdEnum::ContentRemoteRSSI)
				{
					LinkInfo->SetPartnerRSSINormalized((uint8_t)content);
					SetNextRunASAP();
				}
				break;
			default:
				break;
			}
		}
	}

	void OnLinkingSwitchOverReceived(const uint8_t requestId, const uint8_t subHeader)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::InfoSyncStage &&
			subHeader == LOLA_LINK_SUBHEADER_ACK_INFO_SYNC_ADVANCE)
		{
			HostInfoTransaction.OnAdvanceRequestReceived(requestId);
			SetNextRunASAP();
		}
	}
	///

	///Protocol promotion to connection!
	void OnLinkProtocolSwitchOver()
	{
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
		{
			PrepareLinkProtocolSwitchOver();
			RequestSendPacket(true);
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		}
	}
	///

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			NewSession();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			SetNextRunDelay(LOLA_LINK_SERVICE_UNLINK_HOST_SLEEP_PERIOD);
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			HostChallengeTransaction.NewRequest();
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
			HostClockSyncTransaction.Reset();
			break;
		default:
			break;
		}
	}

	void OnClearSession()
	{
		LatencyMeter.Reset();
		SessionLastStarted = ILOLA_INVALID_MILLIS;
	}

	void OnPreSend()
	{
		if (PacketHolder.GetDefinition()->HasACK())
		{
			//Piggy back on any link with ack packets to measure latency.
			LatencyMeter.OnAckPacketSent(PacketHolder.GetId());
		}
	}

	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		//Catch and store acked packets' round trip time.
		LatencyMeter.OnAckReceived(id);

		if (header == LinkWithAckDefinition.GetHeader())
		{
			OnLinkPacketAckReceived(id);
		}
	}
};
#endif