// LoLaLinkHostService.h

#ifndef _LOLA_LINK_HOST_SERVICE_h
#define _LOLA_LINK_HOST_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

class LoLaLinkHostService : public LoLaLinkService
{
private:
	enum AwaitingConnectionEnum
	{
		Starting = 0,
		BroadcastingOpenSession = 1,
		GotResponse = 2,
		ResponseOk = 3,
		SendingResponseOk = 4,
		ResponseNotOk = 5,
	};

public:
	LoLaLinkHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		LinkPMAC = LOLA_LINK_HOST_PMAC;
	}
protected:

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection Host service"));
	}
#endif // DEBUG_LOLA

	bool ShouldProcessPackets()
	{
		return LoLaLinkService::ShouldProcessPackets();
	}

	void OnLinkWarningLow()
	{
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_MIN_ELAPSED_BEFORE_HELLO)
		{
			PrepareHello();
			RequestSendPacket();
		}
	}

	void OnLinkWarningMedium()
	{
		PrepareHello();
		RequestSendPacket();
	}

	void OnChallengeReplyReceived(const uint8_t sessionId, uint8_t* data)
	{
		ATUI.array[0] = data[0];
		ATUI.array[1] = data[1];
		ATUI.array[2] = data[2];
		ATUI.array[3] = data[3];

		switch (LinkInfo.LinkState)
		{

		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			if (ConnectingState == AwaitingConnectionEnum::BroadcastingOpenSession ||
				ConnectingState == AwaitingConnectionEnum::Starting)
			{
				RemotePMAC = ATUI.uint;

				ConnectingState = AwaitingConnectionEnum::GotResponse;
				SetNextRunASAP();
			}
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

	void OnHelloReceived(const uint8_t sessionId, uint8_t* data)
	{
		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::Setup:
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
		case LoLaLinkInfo::LinkStateEnum::Connected:
			ATUI.array[0] = data[0];
			ATUI.array[1] = data[1];
			ATUI.array[2] = data[2];
			ATUI.array[3] = data[3];

			if (SessionId == LOLA_LINK_SERVICE_INVALID_SESSION ||
				RemotePMAC == LOLA_LINK_SERVICE_INVALID_PMAC ||
				(RemotePMAC == ATUI.uint && SessionId != sessionId))
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Disabled:
		default:
			break;
		}
	}

	void OnAwaitingConnection()
	{
		switch (ConnectingState)
		{
		case AwaitingConnectionEnum::Starting:
			ConnectingState = AwaitingConnectionEnum::BroadcastingOpenSession;
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::BroadcastingOpenSession:
			if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				SetNextRunDelay(LOLA_LINK_SERVICE_SLEEP_PERIOD);
			}
			else if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_BROADCAST_PERIOD)
			{
				BroadCast();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_BROADCAST_PERIOD);
			}
			break;
		case AwaitingConnectionEnum::GotResponse:
			if (RemotePMAC != LOLA_LINK_SERVICE_INVALID_PMAC)
			{
				ConnectingState = AwaitingConnectionEnum::ResponseOk;
			}
			else
			{
#ifdef DEBUG_LOLA
				Serial.println(F("ResponseNotOk"));
#endif
				ConnectingState = AwaitingConnectionEnum::ResponseNotOk;
			}
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::ResponseOk:
			PrepareResponseOk();
			RequestSendPacket();
			ConnectingState = AwaitingConnectionEnum::SendingResponseOk;
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::ResponseNotOk:
			ConnectingState = AwaitingConnectionEnum::Starting;
			RemotePMAC = LOLA_LINK_SERVICE_INVALID_PMAC;
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::SendingResponseOk:
			SetNextRunASAP();
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connecting);
			break;
		default:
			ConnectingState = AwaitingConnectionEnum::ResponseNotOk;
			SetNextRunASAP();
			break;
		}
	}

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::Setup:
			SetNextRunASAP();
			NewSession();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			NewSession();
			ConnectingState = AwaitingConnectionEnum::Starting;
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			ConnectingState = AwaitingConnectionEnum::Starting;
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
			ConnectingState = ConnectingEnum::ConnectingStarting;
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::LinkStateEnum::Connected:
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::LinkStateEnum::Disabled:
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

	void BroadCast()
	{
		//#ifdef DEBUG_LOLA
		//		Serial.print(F("B: "));
		//		Serial.println(SessionId);
		//#endif
		PreparePacketBroadcast();
		RequestSendPacket();
	}

	void PreparePacketBroadcast()
	{
		PrepareBasePacketMAC(LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_BROADCAST);
	}

	void PrepareResponseOk()
	{
		PrepareBasePacketMAC(LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED);
	}
};
#endif