// LoLaConnectionHostService.h

#ifndef _LOLACONNECTIONHOSTSERVICE_h
#define _LOLACONNECTIONHOSTSERVICE_h

#include <Services\Connection\LoLaConnectionService.h>

class LoLaConnectionHostService : public LoLaConnectionService
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
	LoLaConnectionHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaConnectionService(scheduler, loLa)
	{
		LinkPMAC = LOLA_CONNECTION_HOST_PMAC;
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
		return LoLaConnectionService::ShouldProcessPackets();
	}

	void OnLinkWarningLow() 
	{
		if (Millis() - TimeHelper > CONNECTION_SERVICE_MIN_ELAPSED_BEFORE_HELLO)
		{
			TimeHelper = Millis();
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

		switch (LinkInfo.State)
		{

		case LoLaLinkInfo::ConnectionState::AwaitingConnection:
		case LoLaLinkInfo::ConnectionState::AwaitingSleeping:
			if (ConnectingState == AwaitingConnectionEnum::BroadcastingOpenSession ||
				ConnectingState == AwaitingConnectionEnum::Starting)
			{
				RemotePMAC = ATUI.uint;

				ConnectingState = AwaitingConnectionEnum::GotResponse;
				SetNextRunASAP();
			}
		case LoLaLinkInfo::ConnectionState::Connected:
			if (SessionId == 0 ||
				RemotePMAC == 0 ||
				RemotePMAC == ATUI.uint)
			{
				UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingConnection);
			}
			break;
		case LoLaLinkInfo::ConnectionState::Connecting:
		case LoLaLinkInfo::ConnectionState::Setup:
		case LoLaLinkInfo::ConnectionState::Disabled:
		default:
			break;
		}
	}

	void OnHelloReceived(const uint8_t sessionId, uint8_t* data)
	{
		switch (LinkInfo.State)
		{
		case LoLaLinkInfo::ConnectionState::Setup:
		case LoLaLinkInfo::ConnectionState::AwaitingConnection:
		case LoLaLinkInfo::ConnectionState::AwaitingSleeping:
			TimeHelper = 0;
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::ConnectionState::Connecting:
		case LoLaLinkInfo::ConnectionState::Connected:
			ATUI.array[0] = data[0];
			ATUI.array[1] = data[1];
			ATUI.array[2] = data[2];
			ATUI.array[3] = data[3];
			TimeHelper = 0;

			if (SessionId == 0 ||
				RemotePMAC == 0 ||
				(RemotePMAC == ATUI.uint && SessionId != sessionId))
			{
				UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingConnection);
			}
			break;
		case LoLaLinkInfo::ConnectionState::Disabled:
		default:
			break;
		}
	}

	void OnConnecting()
	{
		switch (ConnectingState)
		{
		case ConnectingEnum::ConnectingStarting:
			TimeHelper = Millis();
			if (SessionId == 0)
			{
				UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingConnection);
			}
			else
			{
				ConnectingState = ConnectingEnum::Diagnostics;
			}
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
			break;
		case ConnectingEnum::ConnectionGreenLight:
			UpdateLinkState(LoLaLinkInfo::ConnectionState::Connected);
			SetNextRunASAP();
			break;
		case ConnectingEnum::ConnectionEscalationFailed:
		default:
			UpdateLinkState(LoLaLinkInfo::ConnectionState::Setup);
			ConnectingState = AwaitingConnectionEnum::Starting;
			SetNextRunASAP();
			break;
		}
	}

	void OnAwaitingConnection()
	{
		switch (ConnectingState)
		{
		case AwaitingConnectionEnum::Starting:
			TimeHelper = 0;
			ConnectingState = AwaitingConnectionEnum::BroadcastingOpenSession;
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::BroadcastingOpenSession:
			if (GetElapsedSinceStart() > CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
			{
				UpdateLinkState(LoLaLinkInfo::ConnectionState::AwaitingSleeping);
				SetNextRunDelay(CONNECTION_SERVICE_SLEEP_PERIOD);
			}
			else if (Millis() - TimeHelper > LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD)
			{
				BroadCast();
				TimeHelper = Millis();
			}
			else
			{
				SetNextRunDelay(LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD);
			}
			break;
		case AwaitingConnectionEnum::GotResponse:
			if (RemotePMAC != 0)
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
			RemotePMAC = 0;
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::SendingResponseOk:
			SetNextRunASAP();
			UpdateLinkState(LoLaLinkInfo::ConnectionState::Connecting);
			break;
		default:
			ConnectingState = AwaitingConnectionEnum::ResponseNotOk;
			SetNextRunASAP();
			break;
		}
	}

	void OnLinkStateChanged(const LoLaLinkInfo::ConnectionState newState)
	{
		switch (newState)
		{
		case LoLaLinkInfo::ConnectionState::Setup:
			SetNextRunASAP();
			StartTimeReset();
			NewSession();
			break;
		case LoLaLinkInfo::ConnectionState::AwaitingConnection:
			NewSession();
			ConnectingState = AwaitingConnectionEnum::Starting;
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::ConnectionState::AwaitingSleeping:
			ConnectingState = AwaitingConnectionEnum::Starting;
			StartTimeReset();
			break;
		case LoLaLinkInfo::ConnectionState::Connecting:
			ConnectingState = ConnectingEnum::ConnectingStarting;
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::ConnectionState::Connected:
			TimeHelper = 0;
			StartTimeReset();
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::ConnectionState::Disabled:
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
		PrepareBasePacketMAC(CONNECTION_SERVICE_SUBHEADER_CHALLENGE_BROADCAST);
	}

	void PrepareResponseOk()
	{
		PrepareBasePacketMAC(CONNECTION_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED);
	}
};
#endif