// LoLaLinkHostService.h

#ifndef _LOLA_LINK_HOST_SERVICE_h
#define _LOLA_LINK_HOST_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

class LoLaLinkHostService : public LoLaLinkService
{
private:
	enum AwaitingConnectionEnum : uint8_t
	{
		BroadcastingOpenSession = 0,
		LinkRequested = 1,
		ConnectingSwitchOver = 2
	};

	LinkHostClockSyncer ClockSyncer;
	ClockSyncResponseTransaction HostClockSyncTransaction;

	ChallengeRequestTransaction HostChallengeTransaction;

private:
	void NewSession()
	{
		ClearSession();
		SessionId = (uint8_t)random((int32_t)1, (int32_t)(UINT8_MAX - 1));
	}

public:
	LoLaLinkHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		ClockSyncerPointer = &ClockSyncer;
		ClockSyncTransaction = &HostClockSyncTransaction;
		ChallengeTransaction = &HostChallengeTransaction;
		loLa->SetDuplexSlot(false);
	}
protected:

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link Host"));
	}
#endif // DEBUG_LOLA

	//Host version, RemotePMAC is the Remote's PMAC.
	void SetBaseSeed()
	{
		CryptoSeed.SetBaseSeed(PMACGenerator.GetPMAC(), RemotePMAC, SessionId);
	}

	void OnLinkDiscoveryReceived()
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::AwaitingSleeping)
		{
			SetNextRunASAP();
		}
	}

	void OnLinkRequestReceived(const uint8_t sessionId, const uint32_t remotePMAC)
	{
		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (SessionId != LOLA_LINK_SERVICE_INVALID_SESSION &&
				SessionId == sessionId &&
				remotePMAC != LOLA_INVALID_PMAC &&
				ConnectingState == AwaitingConnectionEnum::BroadcastingOpenSession)
			{
				//Here is where we have the first choice to reject this remote.
				//TODO: PMAC Filtering?
				RemotePMAC = remotePMAC;
#ifdef DEBUG_LOLA
				ConnectionProcessStart = millis();
#endif
				SetConnectingState(AwaitingConnectionEnum::LinkRequested);
			}
			break;
		default:
			break;
		}
	}

	void OnLinkRequestReadyReceived(const uint8_t sessionId, const uint32_t remotePMAC)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			ConnectingState == AwaitingConnectionEnum::LinkRequested &&
			SessionId != LOLA_LINK_SERVICE_INVALID_SESSION &&
			SessionId == sessionId &&
			RemotePMAC == remotePMAC)
		{
			SetConnectingState(AwaitingConnectionEnum::ConnectingSwitchOver);
		}
	}

	void OnLinkPacketAckReceived(const uint8_t requestId)
	{
		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (ConnectingState == AwaitingConnectionEnum::ConnectingSwitchOver &&
				RemotePMAC != LOLA_INVALID_PMAC && 
				SessionId == requestId)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connecting);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
			switch (ConnectingState)
			{
			case ConnectingStagesEnum::ClockSyncSwitchOver:
				if (requestId == LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER)
				{
					SetConnectingState(ConnectingStagesEnum::ChallengeStage);
				}
				break;
			case ConnectingStagesEnum::ChallengeSwitchOver:
				if (requestId == ChallengeTransaction->GetTransactionId())
				{
					SetConnectingState(ConnectingStagesEnum::LinkProtocolSwitchOver);
					//The last step is to exchange a packet with TOTP enabled.
					SetTOTPEnabled();
				}
				break;
			case ConnectingStagesEnum::LinkProtocolSwitchOver:
				if (requestId == SessionId)
				{
					SetConnectingState(ConnectingStagesEnum::AllConnectingStagesDone);
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
		switch (ConnectingState)
		{
		case AwaitingConnectionEnum::BroadcastingOpenSession:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_BROADCAST_PERIOD)
			{
				PreparePacketBroadcast();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
			}
			break;
		case AwaitingConnectionEnum::LinkRequested:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				PrepareLinkRequestAccepted();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
			}
			break;
		case AwaitingConnectionEnum::ConnectingSwitchOver:
			if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD)
			{
				//Send the switch-over packet with Ack.
				PrepareLinkConnectingSwitchOver();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
			}
			break;
		default:
			SetConnectingState(0);
			SetNextRunASAP();
			break;
		}
	}

	///Clock Sync.
	void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connecting &&
			ConnectingState == ConnectingStagesEnum::ClockSyncStage)
		{
			HostClockSyncTransaction.SetResult(requestId,
				(int32_t)(ClockSyncer.GetMillisSynced(GetLoLa()->GetLastValidReceivedMillis()) - estimatedMillis));
			SetNextRunASAP();
		}
	}
	
	void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connected)
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
			else
			{
				Serial.print("Clock Sync error: ");
				Serial.println(HostClockSyncTransaction.GetResult());
			}

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
			SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
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
			SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
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
			SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
		}
	}
	///

	///Challenge. Possibly CPU intensive task.
	void OnChallenging()
	{
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD)
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
			SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
		}
	}

	void OnChallengeResponseReceived(const uint8_t requestId, const uint32_t token)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connecting &&
			ConnectingState == ConnectingStagesEnum::ChallengeStage)
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
			SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
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
			SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
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

};
#endif