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
		GotHost = 1
	};

	uint32_t ClockSyncHelper = 0;
	uint32_t LastKeepingClockSynced = 0;
public:
	LoLaLinkRemoteService(Scheduler* scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		LinkPMAC = LOLA_LINK_REMOTE_PMAC;
		loLa->SetDuplexSlot(true);
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection Remote service"));
	}
#endif // DEBUG_LOLA

	//Remote version, RemotePMAC is the Host's PMAC.
	void SetBaseSeed()
	{
		CryptoSeed.SetBaseSeed(RemotePMAC, LinkPMAC, SessionId);
	}

	/*void OnClockSyncWarning()
	{
		if (!SyncedClockIsSynced && Millis() - LastKeepingClockSynced > LOLA_LINK_SERVICE_CLOCK_SYNC_LOOP_PERIOD) {
			LastKeepingClockSynced = Millis();
			PrepareClockSync();
			RequestSendPacket();
		}
	}*/

	//void OnClockReplyReceived(const uint8_t sessionId, uint8_t* data)
	//{
	//	ATUI.array[0] = data[0];
	//	ATUI.array[1] = data[1];
	//	ATUI.array[2] = data[2];
	//	ATUI.array[3] = data[3];

	//	SyncedClock->AddOffset(ATUI.uint);

	//	if (ATUI.uint == 0) 
	//	{
	//		//NTP reports clocks synced.
	//		SyncedClockIsSynced = true;
	//	}
	//	else 
	//	{
	//		//NTP reports clocks not synced.
	//		SyncedClockIsSynced = false;
	//		LastKeepingClockSynced = 0;
	//	}
	//}


	void OnBroadcastReceived(const uint8_t sessionId, const uint32_t remotePMAC)
	{
		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::Connected:
			if (remotePMAC != LOLA_LINK_SERVICE_INVALID_PMAC && RemotePMAC == remotePMAC)
			{
				//We received a broadcats but we thought we were connected.
				//Oh well, better restart the link.
				//Note: This is a source of easy denial of service attack.
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
				SetNextRunASAP();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Setup:
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (ConnectingState = AwaitingConnectionEnum::SearchingForHost &&
				remotePMAC != LOLA_LINK_SERVICE_INVALID_PMAC &&
				sessionId != LOLA_LINK_SERVICE_INVALID_SESSION)
			{
				//Here is where we have the choice to connect or not to this host.
				//TODO: PMAC Filtering?
				//TODO: User UI choice?
				RemotePMAC = remotePMAC;
				SessionId = sessionId;
				ConnectingState = AwaitingConnectionEnum::GotHost;
				SetNextRunASAP();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
		default:
			break;
		}
	}

	void OnLinkRequestAcceptedReceived(const uint32_t token)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			ConnectingState == AwaitingConnectionEnum::GotHost)
		{
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connecting);
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
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD)
			{
				//Send an Hello to wake up potential hosts.
				PrepareHello();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDefault();
			}
			break;
		case AwaitingConnectionEnum::GotHost:
			if (SessionId == LOLA_LINK_SERVICE_INVALID_SESSION ||
				RemotePMAC == LOLA_LINK_SERVICE_INVALID_PMAC ||
				GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT)
			{
				ConnectingState = AwaitingConnectionEnum::SearchingForHost;
				SetNextRunDefault();
				return;
			}
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD)
			{
				//Send an Hello to wake up potential hosts.
				PrepareLinkRequest();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDefault();
			}
			break;
		default:
			break;
		}
	}

	void OnConnecting()
	{
		switch (ConnectingState)
		{
		case ConnectingStagesEnum::ChallengeStage:
			break;
		case ConnectingStagesEnum::ClockSyncStage:
			break;
		case ConnectingStagesEnum::LinkProtocolStage:
			break;
		default:
			break;
		}
	}

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		if (newState == LoLaLinkInfo::LinkStateEnum::AwaitingLink)
		{
			ClearSession();
		}
	}


private:
	void PrepareLinkRequest()
	{
		PrepareBasePacket(LOLA_LINK_SERVICE_SUBHEADER_REMOTE_LINK_REQUEST);
		ATUI.uint = LinkPMAC;
		ArrayToPayload();
	}
};
#endif