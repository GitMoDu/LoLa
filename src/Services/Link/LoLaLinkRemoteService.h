// LoLaLinkRemoteService.h

#ifndef _LOLA_LINK_REMOTE_SERVICE_h
#define _LOLA_LINK_REMOTE_SERVICE_h

#include <Services\Link\LoLaLinkService.h>

class LoLaLinkRemoteService : public LoLaLinkService
{
private:
	enum AwaitingConnectionEnum
	{
		SearchingForBroadcast = 0,
		GotBroadcast = 1,
		SendingChallenge = 2,
		AwaitingCallengeResponse = 3,
		ResponseOk = 4,
		ResponseNotOk = 5
	};

public:
	LoLaLinkRemoteService(Scheduler* scheduler, ILoLa* loLa)
		: LoLaLinkService(scheduler, loLa)
	{
		LinkPMAC = LOLA_LINK_REMOTE_PMAC;
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection Remote service"));
	}
#endif // DEBUG_LOLA

	void OnHelloReceived(const uint8_t sessionId, uint8_t* data)
	{
		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::Connecting:
		case LoLaLinkInfo::LinkStateEnum::Connected:
			ATUI.array[0] = data[0];
			ATUI.array[1] = data[1];
			ATUI.array[2] = data[2];
			ATUI.array[3] = data[3];

			if (SessionId == 0 ||
				RemotePMAC == 0 ||
				(RemotePMAC == ATUI.uint && SessionId != sessionId))
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingConnection);
				SetNextRunASAP();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Setup:
		case LoLaLinkInfo::LinkStateEnum::AwaitingConnection:
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::LinkStateEnum::Disabled:
		default:
			return;
		}
	}

	void OnBroadcastReceived(const uint8_t sessionId, uint8_t* data)
	{
		ATUI.array[0] = data[0];
		ATUI.array[1] = data[1];
		ATUI.array[2] = data[2];
		ATUI.array[3] = data[3];

		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::Connected:
			if (RemotePMAC == ATUI.uint)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingConnection);
				SetNextRunASAP();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Setup:
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingConnection);
		case LoLaLinkInfo::LinkStateEnum::AwaitingConnection:
			RemotePMAC = ATUI.uint;
			SessionId = sessionId;
			ConnectingState = AwaitingConnectionEnum::GotBroadcast;
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
		default:
			//Ignore, nothing to do.
			break;
		}
	}

	void OnChallengeAcceptedReceived(const uint8_t sessionId, uint8_t* data)
	{
		if (ConnectingState == AwaitingConnectionEnum::AwaitingCallengeResponse)
		{
//#ifdef DEBUG_LOLA
//			Serial.println(F("Challenge Accepted"));
//#endif
			ConnectingState = AwaitingConnectionEnum::ResponseOk;
			SetNextRunASAP();
		}
	}

	void OnConnecting()
	{
		if (SessionId == 0)
		{
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingConnection);
			SetNextRunASAP();
			return;
		}

		switch (ConnectingState)
		{
		case ConnectingEnum::ConnectingStarting:
			TimeHelper = Millis();
			ConnectingState = ConnectingEnum::Diagnostics;
			SetNextRunASAP();
			break;
		case ConnectingEnum::Diagnostics:
			ConnectingState = ConnectingEnum::MeasuringLatency;
			LatencyService.RequestRefreshPing();
			SetNextRunDelay(LOLA_LATENCY_SERVICE_UNABLE_TO_COMMUNICATE_TIMEOUT_MILLIS + 1000);
			break;
		case ConnectingEnum::MeasuringLatency:
			ConnectingState = ConnectingEnum::ConnectionEscalationFailed;
			SetNextRunASAP();
			break;
		case ConnectingEnum::MeasurementLatencyDone:
			ConnectingState = ConnectingEnum::ConnectionGreenLight;
			SetNextRunASAP();
			break;
		case ConnectingEnum::ConnectionGreenLight:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connected);
			SetNextRunASAP();
			break;
		case ConnectingEnum::ConnectionEscalationFailed:
		default:
			StartTimeReset();
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingConnection);
			SetNextRunASAP();
			break;
		}
	}

	void OnAwaitingConnection()
	{
		switch (ConnectingState)
		{
		case AwaitingConnectionEnum::SearchingForBroadcast:
			if (GetElapsedSinceStart() > LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				SetNextRunDelay(LOLA_LINK_SERVICE_SLEEP_PERIOD);
			}
			else if (Millis() - TimeHelper > LOLA_LINK_SERVICE_MIN_ELAPSED_BEFORE_HELLO)
			{
				PrepareHello();
				RequestSendPacket();
				TimeHelper = Millis();
			}
			else
			{
				SetNextRunDefault();
			}
			break;
		case AwaitingConnectionEnum::GotBroadcast:
			if (SessionId == 0)
			{
				ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
				SetNextRunDefault();
				return;
			}
			PrepareSendChallenge();
			RequestSendPacket();
			ConnectingState = AwaitingConnectionEnum::SendingChallenge;
			break;
		case AwaitingConnectionEnum::SendingChallenge:
			ConnectingState = AwaitingConnectionEnum::AwaitingCallengeResponse;
			SetNextRunDelay(LOLA_LINK_SERVICE_BROADCAST_PERIOD);
			break;
		case AwaitingConnectionEnum::AwaitingCallengeResponse:
			ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
			SetNextRunDefault();
#ifdef DEBUG_LOLA
			Serial.print(F("CallengeResponse timed out: "));
			Serial.println(SessionId);
#endif
			break;
		case AwaitingConnectionEnum::ResponseOk:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connecting);
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::ResponseNotOk:
			ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
			SetNextRunDelay(1000);
			break;
		default:
			break;
		}
	}

	void OnLinkWarningMedium()
	{
		PrepareHello();
		RequestSendPacket();
	}

	void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::LinkStateEnum::Setup:
			ClearSession();
			StartTimeReset();
			SetNextRunDefault();			
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingConnection:
			ClearSession();
			TimeHelper = 0;
			ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
			break;;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
			ConnectingState = ConnectingEnum::ConnectingStarting;
			break;
		case LoLaLinkInfo::LinkStateEnum::Connected:
			TimeHelper = 0;
			StartTimeReset();
			break;
		case LoLaLinkInfo::LinkStateEnum::Disabled:
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
		default:
			break;
		}
	}

private:
	void PrepareSendChallenge()
	{
		PrepareBasePacketMAC(LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_REPLY);
	}
};
#endif