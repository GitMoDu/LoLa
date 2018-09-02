// LoLaLinkHostService.h

#ifndef _LOLA_LINK_HOST_SERVICE_h
#define _LOLA_LINK_HOST_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

class LoLaLinkHostService : public LoLaLinkService
{
private:
	enum AwaitingConnectionEnum : uint8_t
	{
		Starting = 0,
		BroadcastingOpenSession = 1,
		SwitchOver = 2
	};

	//uint32_t ClockSyncHelper = 0;

public:
	LoLaLinkHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		LinkPMAC = LOLA_LINK_HOST_PMAC;
		loLa->SetDuplexSlot(false);
	}
protected:

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection Host service"));
	}
#endif // DEBUG_LOLA

	//Host version, RemotePMAC is the Remote«s PMAC.
	void SetBaseSeed()
	{
		CryptoSeed.SetBaseSeed(LinkPMAC, RemotePMAC, SessionId);
	}

	//virtual void OnClockReceived(const uint8_t sessionId, uint8_t* data)
	//{
	//	ClockSyncHelper = Millis();
	//	ATUI.array[0] = data[0];
	//	ATUI.array[1] = data[1];
	//	ATUI.array[2] = data[2];
	//	ATUI.array[3] = data[3];

	//	ClockSyncHelper = Millis() - ATUI.uint;

	//	if (ClockSyncHelper == 0)
	//	{
	//		//NTP reports clocks synced.
	//		SyncedClockIsSynced = true;
	//	}
	//	else
	//	{
	//		//NTP reports clocks not synced.
	//		SyncedClockIsSynced = false;
	//	}

	//	//PrepareClockSyncReply(ClockSyncHelper);
	//	//RequestSendPacket();
	//}

	void OnLinkRequestReceived(const uint8_t sessionId, const uint32_t remotePMAC)
	{
		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (SessionId != LOLA_LINK_SERVICE_INVALID_SESSION &&
				SessionId == sessionId &&
				remotePMAC != LOLA_LINK_SERVICE_INVALID_PMAC)
			{
				//Here is where we have the choice to connect or not to this host.
				//TODO: PMAC Filtering?
				//TODO: User UI choice?
				RemotePMAC = remotePMAC;
				ConnectingState = AwaitingConnectionEnum::SwitchOver;
				ConnectingStateStartTime = Millis();
				ResetLastSentTimeStamp();
				SetNextRunASAP();
			}
			else
			{
				//Invalid request, that's not our session.
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::LinkStateEnum::Connected:
			if (SessionId == LOLA_LINK_SERVICE_INVALID_SESSION ||
				RemotePMAC == LOLA_LINK_SERVICE_INVALID_PMAC ||
				RemotePMAC == ATUI.uint)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
		case LoLaLinkInfo::LinkStateEnum::Setup:
		case LoLaLinkInfo::LinkStateEnum::Disabled:
		default:
			break;
		}
	}

	void OnLinkPacketAckReceived()
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::AwaitingLink &&
			ConnectingState == AwaitingConnectionEnum::SwitchOver)
		{
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connecting);
		}
	}

	void OnAwaitingConnection()
	{
		switch (ConnectingState)
		{
		case AwaitingConnectionEnum::Starting:
			ConnectingState = AwaitingConnectionEnum::BroadcastingOpenSession;
			ConnectingStateStartTime = Millis();
			ResetLastSentTimeStamp();
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::BroadcastingOpenSession:
			if (Millis() - ConnectingStateStartTime > LOLA_LINK_SERVICE_BROADCAST_PERIOD)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
			}
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_LINK_RESEND_PERIOD)
			{
				TimeStampLastSent();
				PreparePacketBroadcast();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_BROADCAST_PERIOD);
			}
			break;
		case AwaitingConnectionEnum::SwitchOver:
			if (SessionId == LOLA_LINK_SERVICE_INVALID_SESSION ||
				RemotePMAC == LOLA_LINK_SERVICE_INVALID_PMAC)
			{
				//Go to sleep then wake up to trigger a full link reset.
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				SetNextRunASAP();

			}else if (Millis() - ConnectingStateStartTime > LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT)
			{
				//Go to sleep then wake up to trigger a full link reset.
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				SetNextRunASAP();
			}
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD)
			{
				//Send the switch-over packet with Ack.
				PrepareLinkRequestAccepted();
				RequestSendPacket(true);
			}
			else
			{
				SetNextRunDefault();
			}
			break;
		default:
			ConnectingState = AwaitingConnectionEnum::BroadcastingOpenSession;
			ConnectingStateStartTime = Millis();
			SetNextRunASAP();
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
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			NewSession();
		case LoLaLinkInfo::LinkStateEnum::Connecting:
			ConnectingStateStartTime = Millis();
			break;
		default:
			break;
		}
	}

private:
	void NewSession()
	{
		ClearSession();
		SessionId = 1 + random(0xFE);
	}

	void PrepareLinkRequestAccepted()
	{
		PrepareBasePacketWithAck(LOLA_LINK_SERVICE_SUBHEADER_LINK_REQUEST_ACCEPTED);
		ATUI.uint = 0;
		ArrayToPayload();
	}

	void PreparePacketBroadcast()
	{
		PrepareBasePacket(LOLA_LINK_SERVICE_SUBHEADER_HOST_BROADCAST);
		ATUI.uint = LinkPMAC;
		ArrayToPayload();
	}
};
#endif