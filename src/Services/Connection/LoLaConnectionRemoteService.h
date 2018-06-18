// LoLaConnectionHostService.h

#ifndef _LOLACONNECTIONREMOTESERVICE_h
#define _LOLACONNECTIONREMOTESERVICE_h

#include <Services\Connection\LoLaConnectionService.h>

class LoLaConnectionRemoteService : public LoLaConnectionService
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
	LoLaConnectionRemoteService(Scheduler* scheduler, ILoLa* loLa)
		: LoLaConnectionService(scheduler, loLa)
	{
		LinkPMAC = LOLA_CONNECTION_REMOTE_PMAC;
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
		switch (LinkInfo.State)
		{
		case LoLaLinkInfo::ConnectionState::Connecting:
		case LoLaLinkInfo::ConnectionState::Connected:
			ATUI.array[0] = data[0];
			ATUI.array[1] = data[1];
			ATUI.array[2] = data[2];
			ATUI.array[3] = data[3];

			if (SessionId == 0 ||
				RemotePMAC == 0 ||
				(RemotePMAC == ATUI.uint && SessionId != sessionId))
			{
				UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingConnection);
				SetNextRunASAP();
			}
			break;
		case LoLaLinkInfo::ConnectionState::Setup:
		case LoLaLinkInfo::ConnectionState::AwaitingConnection:
		case LoLaLinkInfo::ConnectionState::AwaitingSleeping:
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::ConnectionState::Disabled:
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

		switch (LinkInfo.State)
		{
		case LoLaLinkInfo::ConnectionState::Connected:
			if (RemotePMAC == ATUI.uint)
			{
				UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingConnection);
				SetNextRunASAP();
			}
			break;
		case LoLaLinkInfo::ConnectionState::Setup:
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::ConnectionState::AwaitingSleeping:
			UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingConnection);
		case LoLaLinkInfo::ConnectionState::AwaitingConnection:
			RemotePMAC = ATUI.uint;
			SessionId = sessionId;
			ConnectingState = AwaitingConnectionEnum::GotBroadcast;
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::ConnectionState::Connecting:
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
			UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingConnection);
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
			UpdateLinkState(LoLaLinkInfo::ConnectionState::Connected);
			SetNextRunASAP();
			break;
		case ConnectingEnum::ConnectionEscalationFailed:
		default:
			StartTimeReset();
			UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingConnection);
			SetNextRunASAP();
			break;
		}
	}

	void OnAwaitingConnection()
	{
		switch (ConnectingState)
		{
		case AwaitingConnectionEnum::SearchingForBroadcast:
			if (GetElapsedSinceStart() > CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
			{
				UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingSleeping);
				SetNextRunDelay(CONNECTION_SERVICE_SLEEP_PERIOD);
			}
			else if (Millis() - TimeHelper > CONNECTION_SERVICE_MIN_ELAPSED_BEFORE_HELLO)
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
			SetNextRunDelay(LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD);
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
			UpdateLinkState(LoLaLinkInfo::ConnectionState::Connecting);
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

	void OnLinkStateChanged(const LoLaLinkInfo::ConnectionState newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::ConnectionState::Setup:
			ClearSession();
			StartTimeReset();
			SetNextRunDefault();			
			break;
		case LoLaLinkInfo::ConnectionState::AwaitingConnection:
			ClearSession();
			TimeHelper = 0;
			ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
			break;;
		case LoLaLinkInfo::ConnectionState::Connecting:
			ConnectingState = ConnectingEnum::ConnectingStarting;
			break;
		case LoLaLinkInfo::ConnectionState::Connected:
			TimeHelper = 0;
			StartTimeReset();
			ConnectingState = ConnectingEnum::Starting;
			break;
		case LoLaLinkInfo::ConnectionState::Disabled:
		case LoLaLinkInfo::ConnectionState::AwaitingSleeping:
		default:
			break;
		}
	}

private:
	void PrepareSendChallenge()
	{
		PrepareBasePacketMAC(CONNECTION_SERVICE_SUBHEADER_CHALLENGE_REPLY);
	}
};
#endif