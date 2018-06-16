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

	enum ConnectingEnum
	{
		ConnectingStarting = 6,
		Diagnostics = 7,
		MeasuringLatency = 8,
		MeasurementLatencyDone = 9,
		ConnectionGreenLight = 10,
		ConnectionEscalationFailed = 11
	};

public:
	LoLaConnectionHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaConnectionService(scheduler, loLa)
	{
		LinkPMAC = LOLA_CONNECTION_HOST_PMAC;
		ConnectingState = AwaitingConnectionEnum::Starting;
	}
protected:

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection Host service"));
	}
#endif // DEBUG_LOLA

	void OnChallengeReply(uint8_t* data)
	{
#ifdef DEBUG_LOLA
		Serial.print(F("OnChallengeReply: "));
		Serial.println(SessionId);
#endif
		if (ConnectingState == AwaitingConnectionEnum::BroadcastingOpenSession)
		{
			ATUI.array[0] = data[0];
			ATUI.array[1] = data[1];
			ATUI.array[2] = data[2];
			ATUI.array[3] = data[3];

			RemotePMAC = ATUI.uint;

			ConnectingState = AwaitingConnectionEnum::GotResponse;
			SetNextRunASAP();
		}
	}

	void OnHelloReceived(uint8_t* data)
	{
#ifdef DEBUG_LOLA
		Serial.println(F("Hi"));
#endif

		switch (ConnectionInfo.State)
		{
		case LoLaLinkInfo::ConnectionState::Setup:
			Enable();
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::ConnectionState::AwaitingConnection:
			Enable();
			SetNextRunDefault();
		break;
		case LoLaLinkInfo::ConnectionState::Connecting:
		case LoLaLinkInfo::ConnectionState::Connected:
			ATUI.array[0] = data[0];
			ATUI.array[1] = data[1];
			ATUI.array[2] = data[2];
			ATUI.array[3] = data[3];

			if (RemotePMAC != 0 && RemotePMAC == ATUI.uint)
			{
				ResetToSetup();
				SetNextRunASAP();
			}
#ifdef DEBUG_LOLA
			else
			{
				Serial.print(F("Mac Mismatch: RemotePMAC: "));
				Serial.print(RemotePMAC);
				Serial.print(F("  Messaged PMAC: "));
				Serial.println(ATUI.uint);
			}
#endif
			break;
		case LoLaLinkInfo::ConnectionState::Disabled:
		default:
			return;
		}
	}

	void OnConnecting()
	{
		switch (ConnectingState)
		{
		case ConnectingEnum::ConnectingStarting:
			TimeHelper = Millis();
#ifdef DEBUG_LOLA
			Serial.println(F("OnConnecting"));
#endif
			if (SessionId == 0)
			{
				DemoteToAwaiting();
				ConnectingState = AwaitingConnectionEnum::BroadcastingOpenSession;
			}
			else
			{
				ConnectingState = ConnectingEnum::Diagnostics;
			}
			SetNextRunASAP();
			break;
		case ConnectingEnum::Diagnostics:
#ifdef DEBUG_LOLA
			Serial.println(F("StartingDiagnostics"));
#endif
			//ConnectingState = ConnectingEnum::MeasuringLatency;
			//LatencyService.RequestRefreshPing();
			//SetNextRunDelay(LOLA_LATENCY_SERVICE_UNABLE_TO_COMMUNICATE_TIMEOUT_MILLIS + 1000);
			ConnectingState = ConnectingEnum::MeasurementLatencyDone;
			SetNextRunDelay(300);
			break;
		case ConnectingEnum::MeasuringLatency:
			ConnectingState = ConnectingEnum::ConnectionEscalationFailed;
			SetNextRunASAP();
			break;
		case ConnectingEnum::MeasurementLatencyDone:
#ifdef DEBUG_LOLA
			Serial.println(F("MeasurementLatencyDone"));
#endif
			ConnectingState = ConnectingEnum::ConnectionGreenLight;
			break;
		case ConnectingEnum::ConnectionGreenLight:
			PromoteToConnected();
			SetNextRunASAP();
			break;
		case ConnectingEnum::ConnectionEscalationFailed:
		default:
			ConnectingState = AwaitingConnectionEnum::BroadcastingOpenSession;
			ResetToSetup();
			SetNextRunASAP();
			break;
		}
	}

	void OnAwaitingConnection()
	{
		switch (ConnectingState)
		{
		case AwaitingConnectionEnum::Starting:
			RemotePMAC = 0;
			TimeHelper = 0;
			ConnectingState = AwaitingConnectionEnum::BroadcastingOpenSession;
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::BroadcastingOpenSession:
			if (Millis() - TimeHelper > LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD)
			{
#ifdef DEBUG_LOLA
				Serial.print(F("B: "));
				Serial.println(SessionId);
#endif
				PreparePacketBroadcast();
				RequestSendPacket();
				TimeHelper = Millis();
			}
			else if (GetElapsedSinceStart() > CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
			{
				ResetToSetup();
				SetNextRunDelay(CONNECTION_SERVICE_SLEEP_PERIOD);
			}
			else
			{
				SetNextRunDelay(LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD);
			}
			break;
		case AwaitingConnectionEnum::GotResponse:
#ifdef DEBUG_LOLA
			Serial.print(F("GotResponse: "));
			Serial.println(SessionId);
#endif
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
#ifdef DEBUG_LOLA
			Serial.println(F("ResponseOk:"));
#endif
			PrepareResponseOk();
			RequestSendPacket();
			ConnectingState = AwaitingConnectionEnum::SendingResponseOk;
			SetNextRunASAP();
			break;
		case AwaitingConnectionEnum::ResponseNotOk:
			ConnectingState = AwaitingConnectionEnum::Starting;
			RemotePMAC = 0;
			SetNextRunDefault();
			break;
		case AwaitingConnectionEnum::SendingResponseOk:
			SetNextRunASAP();
			ConnectingState = ConnectingEnum::ConnectingStarting;
			PromoteToConnecting();
			break;
		default:
			ConnectingState = AwaitingConnectionEnum::ResponseNotOk;
			SetNextRunASAP();
			break;
		}
	}

	void OnConnectionSetup()
	{
		NewSessionId();
		RemotePMAC = 0;
		SetNextRunDefault();
		ConnectingState = AwaitingConnectionEnum::Starting;
	}

	void OnKeepingConnected(const uint32_t elapsedSinceLastReceived)
	{
		LoLaConnectionService::OnKeepingConnected(elapsedSinceLastReceived);
	}

private:
	void OnLatencyMeasurementComplete(const bool success)
	{
		if (ConnectingState == ConnectingEnum::MeasuringLatency)
		{
			if (success)
			{
				ConnectingState = ConnectingEnum::MeasurementLatencyDone;
			}
			else
			{
				ConnectingState = ConnectingEnum::ConnectionEscalationFailed;
			}
			SetNextRunASAP();
		}
	}

	void NewSessionId()
	{
		SessionId = 1 + random(0xFE);
	}

	void PreparePacketBroadcast()
	{
		PrepareBasePacketMAC(LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_BROADCAST);
	}

	void PrepareResponseOk()
	{
		PrepareBasePacketMAC(LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED);
	}
};
#endif