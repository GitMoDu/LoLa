// LoLaLinkRemoteService.h

#ifndef _LOLA_LINK_REMOTE_SERVICE_h
#define _LOLA_LINK_REMOTE_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

class LoLaLinkRemoteService : public LoLaLinkService
{
private:
	enum AwaitingConnectionEnum
	{
		SearchingForHost = 0,
		GotHost = 1,
		AcknowledgingHost = 2,
		WaitingForSwitchOver = 3
	};

	LinkRemoteClockSyncer ClockSyncer;
	ClockSyncRequestTransaction RemoteClockSyncTransaction;

	ChallengeReplyTransaction RemoteChallengeTransaction;

public:
	LoLaLinkRemoteService(Scheduler* scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		ClockSyncerPointer = &ClockSyncer;
		ClockSyncTransaction = &RemoteClockSyncTransaction;
		ChallengeTransaction = &RemoteChallengeTransaction;
		loLa->SetDuplexSlot(true);
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Remote"));
	}
#endif // DEBUG_LOLA

	//Remote version, RemotePMAC is the Host's PMAC.
	void SetBaseSeed()
	{
		CryptoSeed.SetBaseSeed(RemotePMAC, PMACGenerator.GetPMAC(), SessionId);
	}

	void OnBroadcastReceived(const uint8_t sessionId, const uint32_t remotePMAC)
	{
		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::Connected:
			if (remotePMAC != LOLA_INVALID_PMAC && RemotePMAC == remotePMAC)
			{
				//We received a broadcats but we thought we were connected.
				//Oh well, better restart the link.
				//Note: This is a source of easy denial of service attack.
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
				SetNextRunASAP();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (ConnectingState == AwaitingConnectionEnum::SearchingForHost &&
				remotePMAC != LOLA_INVALID_PMAC &&
				sessionId != LOLA_LINK_SERVICE_INVALID_SESSION)
			{
				//Here is where we have the choice to connect or not to this host.
				//TODO: PMAC Filtering?
				//TODO: User UI choice?
				RemotePMAC = remotePMAC;
				SessionId = sessionId;
				SetConnectingState(AwaitingConnectionEnum::GotHost);
			}
			break;
		default:
			break;
		}
	}

	void OnLinkRequestAcceptedReceived(const uint8_t requestId, const uint32_t localPMAC)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			ConnectingState == AwaitingConnectionEnum::GotHost &&
			PMACGenerator.GetPMAC() == localPMAC)
		{
#ifdef DEBUG_LOLA
			ConnectionProcessStart = millis();
#endif
			SetConnectingState(AwaitingConnectionEnum::AcknowledgingHost);
		}
	}

	void OnLinkSwitchOverReceived(const uint8_t requestId, const uint8_t subHeader)
	{
		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (subHeader == LOLA_LINK_SUBHEADER_ACK_LINK_REQUEST_SWITCHOVER &&
				ConnectingState == AwaitingConnectionEnum::AcknowledgingHost)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connecting);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
			switch (ConnectingState)
			{
			case ConnectingStagesEnum::ClockSyncStage:
				//If we break here, we need to receive two of the same protocol packet.
			case ConnectingStagesEnum::ClockSyncSwitchOver:
				if (subHeader == LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER &&
					requestId == LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER)
				{
					ClockSyncer.SetSynced();
					SetConnectingState(ConnectingStagesEnum::ChallengeStage);
				}
				break;
			case ConnectingStagesEnum::ChallengeStage:
				if (RemoteChallengeTransaction.OnReplyAccepted(requestId))
				{
					SetConnectingState(ConnectingStagesEnum::ChallengeSwitchOver);
				}
				else
				{
					break;
				}
				//If we break here, we need to receive two of the same protocol packet.
			case ConnectingStagesEnum::ChallengeSwitchOver:
				if (subHeader == LOLA_LINK_SUBHEADER_ACK_CHALLENGE_SWITCHOVER &&
					RemoteChallengeTransaction.IsChallengeComplete())
				{
					SetConnectingState(ConnectingStagesEnum::LinkProtocolSwitchOver);
					//The last step is to exchange a packet with TOTP enabled.
					SetTOTPEnabled();
				}
				break;
			case ConnectingStagesEnum::LinkProtocolSwitchOver:
				if (subHeader == LOLA_LINK_SUBHEADER_ACK_PROTOCOL_SWITCHOVER &&
					requestId == SessionId)
				{
					SetConnectingState(ConnectingStagesEnum::AllConnectingStagesDone);
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
		switch (ConnectingState)
		{
		case AwaitingConnectionEnum::SearchingForHost:
			if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
			}
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD)
			{
				//Send an Hello to wake up potential hosts.
				PrepareLinkDiscovery();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
			}
			break;
		case AwaitingConnectionEnum::GotHost:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkRequest();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
			}
			break;
		case AwaitingConnectionEnum::AcknowledgingHost:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkRequestReady();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
			}
			break;
		case AwaitingConnectionEnum::WaitingForSwitchOver:
			SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
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
			SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
		}
	}

	void OnChallengeRequestReceived(const uint8_t requestId, const uint32_t token)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connecting &&
			ConnectingState == ConnectingStagesEnum::ChallengeStage)
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
			ClockSyncer.OnTuneErrorReceived(RemoteClockSyncTransaction.GetResult());

			RemoteClockSyncTransaction.Reset();
			SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
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
				SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
			}
		}
		//else if (false)
		//{
		//	//TODO: Link info update.
		//}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD);
		}
	}

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
			SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
		}
	}
	
	void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedError)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connecting &&
			ConnectingState == ConnectingStagesEnum::ClockSyncStage &&
			RemoteClockSyncTransaction.IsRequested() &&
			RemoteClockSyncTransaction.GetId() == requestId)
		{
			RemoteClockSyncTransaction.SetResult(estimatedError);
			SetNextRunASAP();
		}
	}

	void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedError)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connected &&
			RemoteClockSyncTransaction.IsRequested() &&
			RemoteClockSyncTransaction.GetId() == requestId)
		{
			RemoteClockSyncTransaction.SetResult(estimatedError);
			SetNextRunASAP();
		}
	}

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			ClearSession();
			break;
		case LoLaLinkInfo::LinkStateEnum::Connected:
			RemoteClockSyncTransaction.Reset();
			break;
		default:
			break;
		}
	}

	void OnPreSend()
	{
		LoLaLinkService::OnPreSend();

		if (PacketHolder.GetDataHeader() == LinkDefinition.GetHeader() &&
			(PacketHolder.GetPayload()[0] == LOLA_LINK_SUBHEADER_NTP_REQUEST ||
				PacketHolder.GetPayload()[0] == LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST))
		{
			//If we are sending a clock sync request, we update our synced clock payload as late as possible.
			ATUI_S.uint = ClockSyncer.GetMillisSync();
			ArrayToPayload();
		}
	}
};
#endif