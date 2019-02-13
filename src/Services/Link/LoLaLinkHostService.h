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
	LoLaLinkLatencyMeter LatencyMeter;

private:
	void NewSession()
	{
		ClearSession();
		LinkInfo->SetSessionId((uint8_t)random(1, (UINT8_MAX - 1)));
	}

public:
	LoLaLinkHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		ClockSyncerPointer = &ClockSyncer;
		ClockSyncTransaction = &HostClockSyncTransaction;
		ChallengeTransaction = &HostChallengeTransaction;
		InfoTransaction = &HostInfoTransaction;
		loLa->SetDuplexSlot(false);
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
		CryptoSeed.SetBaseSeed(PMACGenerator.GetPMAC(), RemotePMAC, SessionId);
	}

	void OnLinkDiscoveryReceived()
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingSleeping)
		{
			SetNextRunASAP();
		}
	}

	void OnLinkRequestReceived(const uint8_t sessionId, const uint32_t remotePMAC)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (SessionId != LOLA_LINK_SERVICE_INVALID_SESSION &&
				SessionId == sessionId &&
				remotePMAC != LOLA_INVALID_PMAC &&
				LinkingState == AwaitingLinkEnum::BroadcastingOpenSession)
			{
				//Here is where we have the first choice to reject this remote.
				//TODO: PMAC Filtering?
				RemotePMAC = remotePMAC;
#ifdef DEBUG_LOLA
				ConnectionProcessStart = millis();
#endif
				SetLinkingState(AwaitingLinkEnum::LinkRequested);
			}
			break;
		default:
			break;
		}
	}

	void OnLinkRequestReadyReceived(const uint8_t sessionId, const uint32_t remotePMAC)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			LinkingState == AwaitingLinkEnum::LinkRequested &&
			RemotePMAC == remotePMAC)
			LinkInfo->HasSessionId() &&
			LinkInfo->GetSessionId() == sessionId &&
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
				RemotePMAC != LOLA_INVALID_PMAC &&
				LinkInfo->GetSessionId() == requestId)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connecting);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
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
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_BROADCAST_PERIOD)
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
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Connecting &&
			LinkingState == LinkingStagesEnum::ClockSyncStage)
		{
			HostClockSyncTransaction.SetResult(requestId,
				(int32_t)(ClockSyncer.GetMillisSynced(GetLoLa()->GetLastValidReceivedMillis()) - estimatedMillis));
			SetNextRunASAP();
		}
	}

	void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Connected)
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
				Serial.print("Clock Sync error: ");
				Serial.println(HostClockSyncTransaction.GetResult());
			}
#endif

			ClockSyncer.OnEstimationReceived(HostClockSyncTransaction.GetResult());

			PrepareClockSyncTuneResponse(HostClockSyncTransaction.GetId(), ClockSyncer.GetLastError());
			HostClockSyncTransaction.Reset();
			RequestSendPacket();
		}
		//else if (false)
		//{
		//	//TODO: Link info update.
		//}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
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
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_LINK_RESEND_PERIOD)
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
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Connecting &&
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
			LinkInfo->SetRTT(LatencyMeter.GetAverageLatency());
			HostInfoTransaction.Advance();//First move is done by host.
			SetNextRunASAP();
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
			if (LinkInfo->HasRemoteRSSI() && GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
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
					LinkInfo->SetRemoteRSSINormalized((uint8_t)content);
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
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Connecting &&
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
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			NewSession();
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
			HostChallengeTransaction.NewRequest();
			break;
		case LoLaLinkInfo::LinkStateEnum::Connected:
			HostClockSyncTransaction.Reset();
			break;
		default:
			break;
		}
	}

	void OnClearSession()
	{
		LatencyMeter.Reset();
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