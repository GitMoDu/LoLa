// LoLaLinkRemoteService.h

#ifndef _LOLA_LINK_REMOTE_SERVICE_h
#define _LOLA_LINK_REMOTE_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

class LoLaLinkRemoteService : public LoLaLinkService
{
private:
	enum AwaitingLinkEnum
	{
		SearchingForHost = 0,
		GotHost = 1,
		AcknowledgingHost = 2,
		WaitingForSwitchOver = 3
	};

	LinkRemoteClockSyncer ClockSyncer;
	ClockSyncRequestTransaction RemoteClockSyncTransaction;

	ChallengeReplyTransaction RemoteChallengeTransaction;

	RemoteInfoSyncTransaction RemoteInfoTransaction;

public:
	LoLaLinkRemoteService(Scheduler* scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		ClockSyncerPointer = &ClockSyncer;
		ClockSyncTransaction = &RemoteClockSyncTransaction;
		ChallengeTransaction = &RemoteChallengeTransaction;
		InfoTransaction = &RemoteInfoTransaction;
		loLa->SetDuplexSlot(true);
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Remote"));
	}
#endif // DEBUG_LOLA

	//Remote version, ParnerMAC is the Host's MAC.
	void SetBaseSeed()
	{
		CryptoSeed.SetBaseSeed(LinkInfo->GetPartnerMAC(), LinkInfo->GetLocalMAC(), LOLA_LINK_INFO_MAC_LENGTH,  LinkInfo->GetSessionId());
	}

	void OnBroadcastReceived(const uint8_t sessionId, uint8_t * hostMAC)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (LinkingState == AwaitingLinkEnum::SearchingForHost &&
				!LinkInfo->HasSessionId() &&
				!LinkInfo->HasPartnerMAC())
			{
				//Here is where we have the choice to connect or not to this host.
				//TODO: MAC Filtering?
				//TODO: User UI choice?
				LinkInfo->SetPartnerMAC(hostMAC);
				LinkInfo->SetSessionId(sessionId);
				SetLinkingState(AwaitingLinkEnum::GotHost);
			}
			break;
		default:
			break;
		}
	}

	void OnLinkRequestAcceptedReceived(const uint8_t requestId, uint8_t* localMAC)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			LinkingState == AwaitingLinkEnum::GotHost &&
			LinkInfo->HasSessionId() &&
			LinkInfo->HasPartnerMAC() && 
			MacManager.Match(localMAC))
		{
			SetLinkingState(AwaitingLinkEnum::AcknowledgingHost);
		}
	}

	void OnLinkingSwitchOverReceived(const uint8_t requestId, const uint8_t subHeader)
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (subHeader == LOLA_LINK_SUBHEADER_ACK_LINK_REQUEST_SWITCHOVER &&
				LinkingState == AwaitingLinkEnum::AcknowledgingHost)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linking);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			switch (LinkingState)
			{
			case LinkingStagesEnum::ClockSyncStage:
				//If we break here, we need to receive two of the same protocol packet.
			case LinkingStagesEnum::ClockSyncSwitchOver:
				if (subHeader == LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER &&
					requestId == LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER)
				{
					ClockSyncer.SetSynced();
					SetLinkingState(LinkingStagesEnum::ChallengeStage);
				}
				break;
			case LinkingStagesEnum::ChallengeStage:
				if (RemoteChallengeTransaction.OnReplyAccepted(requestId))
				{
					SetLinkingState(LinkingStagesEnum::ChallengeSwitchOver);
				}
				else
				{
					break;
				}
				//If we break here, we need to receive two of the same protocol packet.
			case LinkingStagesEnum::ChallengeSwitchOver:
				if (subHeader == LOLA_LINK_SUBHEADER_ACK_CHALLENGE_SWITCHOVER &&
					RemoteChallengeTransaction.IsComplete())
				{
					SetLinkingState(LinkingStagesEnum::InfoSyncStage);
				}
				break;
			case LinkingStagesEnum::InfoSyncStage:				
				if (subHeader == LOLA_LINK_SUBHEADER_ACK_INFO_SYNC_ADVANCE)
				{
					RemoteInfoTransaction.OnAdvanceRequestReceived(requestId);
					SetNextRunASAP();
				}
				break;
			case LinkingStagesEnum::LinkProtocolSwitchOver:
				if (subHeader == LOLA_LINK_SUBHEADER_ACK_PROTOCOL_SWITCHOVER &&
					LinkInfo->GetSessionId() == requestId)
				{
					SetLinkingState(LinkingStagesEnum::AllConnectingStagesDone);
				}
				break;
			default:
				break;
			}
		default:
			break;
		}
	}

	void OnAwaitingConnection()
	{
		switch (LinkingState)
		{
		case AwaitingLinkEnum::SearchingForHost:
			if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
			}
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				//Send an Hello to wake up potential hosts.
				PrepareLinkDiscovery();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::GotHost:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkRequest();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::AcknowledgingHost:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkRequestReady();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case AwaitingLinkEnum::WaitingForSwitchOver:
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
			break;
		default:
			break;
		}
	}

	//Possibly CPU intensive task.
	void OnChallenging()
	{
		if (RemoteChallengeTransaction.IsReplyReady() && GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
		{
			PrepareChallengeReply();
			RequestSendPacket(true);
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
		}
	}

	void OnChallengeRequestReceived(const uint8_t requestId, const uint32_t token)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ChallengeStage)
		{
			RemoteChallengeTransaction.Clear();
			RemoteChallengeTransaction.OnRequest(requestId, token);
			SetNextRunASAP();
		}
	}

	void OnKeepingConnected()
	{
		if (RemoteClockSyncTransaction.IsResultWaiting())
		{
			if (!ClockSyncer.OnTuneErrorReceived(RemoteClockSyncTransaction.GetResult()))
			{
				LinkInfo->StampClockSyncAdjustment();
			}

			RemoteClockSyncTransaction.Reset();
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
		}
		else if (ClockSyncer.IsTimeToTune())
		{
			if (!RemoteClockSyncTransaction.IsRequested())
			{
				RemoteClockSyncTransaction.Reset();
				PrepareClockSyncTuneRequest(RemoteClockSyncTransaction.GetId());
				RemoteClockSyncTransaction.SetRequested();
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
			}
		}
		//else if (false)
		//{
		//	//TODO: Link info update.
		//}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
		}
	}

	///Clock Sync
	void OnClockSync()
	{
		if (RemoteClockSyncTransaction.IsResultWaiting())
		{
			ClockSyncer.OnEstimationErrorReceived(RemoteClockSyncTransaction.GetResult());
			RemoteClockSyncTransaction.Reset();
			SetNextRunASAP();
		}
		else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
		{
			RemoteClockSyncTransaction.Reset();
			PrepareClockSyncRequest(RemoteClockSyncTransaction.GetId());
			RemoteClockSyncTransaction.SetRequested();//TODO: Only set requested on OnSendOk, to reduce the possible DOS window.
			RequestSendPacket(true);
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD);
		}
	}

	void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedError)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::ClockSyncStage &&
			RemoteClockSyncTransaction.IsRequested() &&
			RemoteClockSyncTransaction.GetId() == requestId)
		{
			RemoteClockSyncTransaction.SetResult(estimatedError);
			SetNextRunASAP();
		}
	}

	void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedError)
	{
		if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linked &&
			RemoteClockSyncTransaction.IsRequested() &&
			RemoteClockSyncTransaction.GetId() == requestId)
		{
			RemoteClockSyncTransaction.SetResult(estimatedError);
			SetNextRunASAP();
		}
	}
	///

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			ClearSession();
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
			RemoteClockSyncTransaction.Reset();
			break;
		default:
			break;
		}
	}

	void OnPreSend()
	{
		if (PacketHolder.GetDataHeader() == LinkDefinition.GetHeader() &&
			(PacketHolder.GetPayload()[0] == LOLA_LINK_SUBHEADER_NTP_REQUEST ||
				PacketHolder.GetPayload()[0] == LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST))
		{
			//If we are sending a clock sync request, we update our synced clock payload as late as possible.
			ATUI_S.uint = ClockSyncer.GetMillisSync();
			ArrayToPayload();
		}
	}

	///Info Sync
	void OnInfoSync()
	{
		switch (RemoteInfoTransaction.GetStage())
		{
		case InfoSyncTransaction::StageEnum::StageStart:
			//We wait in this state until we received the first Stage update.
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			break;
		case InfoSyncTransaction::StageEnum::StageHostRTT:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkInfoSyncAdvanceRequest(InfoSyncTransaction::ContentIdEnum::ContentHostRTT);
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case InfoSyncTransaction::StageEnum::StageHostRSSI:
			if (LinkInfo->HasPartnerRSSI())
			{
				if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
				{
					PrepareLinkInfoSyncAdvanceRequest(InfoSyncTransaction::ContentIdEnum::ContentHostRSSI);
					RequestSendPacket(true);
				}
				else
				{
					SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
				}
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
			break;
		case InfoSyncTransaction::StageEnum::StageRemoteRSSI:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkInfoSyncUpdate(InfoSyncTransaction::ContentIdEnum::ContentRemoteRSSI, LinkInfo->GetRSSINormalized());
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
			RemoteInfoTransaction.Clear();
			break;
		}
	}

	void OnLinkInfoSyncUpdateReceived(const uint8_t contentId, const uint32_t content)
	{
		if (RemoteInfoTransaction.OnUpdateReceived(contentId))
		{
			//We don't check for Stage here because OnUpdateReceived already validated Stage.
			switch (contentId)
			{
			case InfoSyncTransaction::ContentIdEnum::ContentHostRTT:
				LinkInfo->SetRTT((uint16_t)content);
				break;
			case InfoSyncTransaction::ContentIdEnum::ContentHostRSSI:
				LinkInfo->SetPartnerRSSINormalized((uint8_t)content);
				break;
			default:
				break;
			}
			SetNextRunASAP();
		}		
	}

	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		if (header == LinkWithAckDefinition.GetHeader() && 
			LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linking &&
			LinkingState == LinkingStagesEnum::InfoSyncStage)
		{
			RemoteInfoTransaction.OnRequestAckReceived(id);
			SetNextRunASAP();
		}
	}
	///
};
#endif