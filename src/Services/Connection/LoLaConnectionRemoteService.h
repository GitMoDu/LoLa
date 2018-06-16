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

	enum ConnectingEnum
	{
		Starting = 6,
		Diagnostics = 7,
		MeasuringLatency = 8,
		ConnectionGreenLight = 9,
		ConnectionEscalationFailed = 10,
	};

public:
	LoLaConnectionRemoteService(Scheduler* scheduler, ILoLa* loLa)
		: LoLaConnectionService(scheduler, loLa)
	{
		LinkPMAC = LOLA_CONNECTION_REMOTE_PMAC;
		ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection Remote service"));
	}
#endif // DEBUG_LOLA
	void OnConnectionSetup()
	{
		ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
		SessionId = 0;
	}

	bool ShouldProcessPackets()
	{
		if (!LoLaConnectionService::ShouldProcessPackets())
		{
			return ConnectingState == AwaitingConnectionEnum::SearchingForBroadcast;
		}
		return true;
	}

	void OnReceivedBroadcast(const uint8_t sessionId, uint8_t* data)
	{
		ATUI.array[0] = data[0];
		ATUI.array[1] = data[1];
		ATUI.array[2] = data[2];
		ATUI.array[3] = data[3];

		switch (ConnectionInfo.State)
		{
		case LoLaLinkInfo::ConnectionState::Connected:
#ifdef DEBUG_LOLA
			Serial.print(F("GotBroadcast Connected State: "));
			Serial.println(ConnectionInfo.State);
#endif
			if (RemotePMAC == ATUI.uint)
			{
				DemoteToDisconnected();
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
		case LoLaLinkInfo::ConnectionState::Connecting:
#ifdef DEBUG_LOLA
			Serial.print(F("GotBroadcast Connecting State: "));
			Serial.println(ConnectionInfo.State);
#endif
			if (RemotePMAC == ATUI.uint)
			{
				DemoteToAwaiting();
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
		case LoLaLinkInfo::ConnectionState::Setup:
#ifdef DEBUG_LOLA
			Serial.print(F("GotBroadcast Setup State: "));
			Serial.println(ConnectionInfo.State);
#endif
			SetNextRunASAP();
			break;
		case LoLaLinkInfo::ConnectionState::AwaitingConnection:
			//TODO: Event warning of new Host to connect.
			RemotePMAC = (uint32_t)data[0] & 0x00FF;
			RemotePMAC += ((uint32_t)data[1]) << 8;
			RemotePMAC += ((uint32_t)data[2]) << 16;
			RemotePMAC += ((uint32_t)data[3]) << 24;

			SessionId = sessionId;

#ifdef DEBUG_LOLA
			Serial.println(F("GotBroadcast OK."));
#endif
			ConnectingState = AwaitingConnectionEnum::GotBroadcast;
			SetNextRunASAP();
			break;
		default:
#ifdef DEBUG_LOLA
			Serial.print(F("GotBroadcast ? State: "));
			Serial.println(ConnectionInfo.State);
#endif
			//Ignore, nothing to do.
			break;
		}
	}

	void OnChallengeAccepted(uint8_t* data)
	{
#ifdef DEBUG_LOLA
		Serial.println(F("OnChallengeAccepted: "));
#endif
		if (ConnectingState == AwaitingConnectionEnum::AwaitingCallengeResponse)
		{
#ifdef DEBUG_LOLA
			Serial.println(F("ResponseOk: "));
#endif
			ConnectingState = AwaitingConnectionEnum::ResponseOk;
			SetNextRunASAP();
		}
	}

	void OnConnecting()
	{
		switch (ConnectingState)
		{
		case ConnectingEnum::Starting:
			TimeHelper = Millis();
			if (SessionId == 0)
			{
				DemoteToAwaiting();
				ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
				SetNextRunASAP();
				return;
			}
			ConnectingState = ConnectingEnum::Diagnostics;
			SetNextRunASAP();
			break;
		case ConnectingEnum::Diagnostics:
			if (SessionId == 0)
			{
				DemoteToAwaiting();
				ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
				SetNextRunASAP();
				return;
			}
#ifdef DEBUG_LOLA
			Serial.print(F("Connecting: "));
			Serial.println(SessionId);
#endif
			//ConnectingState = ConnectingEnum::MeasuringLatency;
			//LatencyService.RequestRefreshPing();
			//SetNextRunDelay(LOLA_LATENCY_SERVICE_UNABLE_TO_COMMUNICATE_TIMEOUT_MILLIS + 1000);
			ConnectingState = ConnectingEnum::ConnectionGreenLight;
			SetNextRunDelay(300);
			break;
		case ConnectingEnum::MeasuringLatency:
			ConnectingState = ConnectingEnum::ConnectionEscalationFailed;
			SetNextRunASAP();
			break;
		case ConnectingEnum::ConnectionGreenLight:
			PromoteToConnected();
			SetNextRunASAP();
			break;
		case ConnectingEnum::ConnectionEscalationFailed:
		default:
			DemoteToAwaiting();
			ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
			SetNextRunASAP();
			break;
		}
	}

	void OnAwaitingConnection()
	{
		switch (ConnectingState)
		{
		case AwaitingConnectionEnum::SearchingForBroadcast:
			if (Millis() - TimeHelper > CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING)
			{
#ifdef DEBUG_LOLA
				Serial.println(F("Hello!"));
#endif
				PrepareHello();
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
		case AwaitingConnectionEnum::GotBroadcast:
			if (SessionId == 0)
			{
				ConnectingState = AwaitingConnectionEnum::SearchingForBroadcast;
				SetNextRunDefault();
				return;
			}
#ifdef DEBUG_LOLA
			Serial.print(F("GotBroadcast, sending challenge: "));
			Serial.println(SessionId);
#endif
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
			PromoteToConnecting();
			ConnectingState = ConnectingEnum::Starting;
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

	void OnKeepingConnected(const uint32_t elapsedSinceLastReceived)
	{
		LoLaConnectionService::OnKeepingConnected(elapsedSinceLastReceived);
	}


private:
	void PrepareHello()
	{
		PrepareBasePacketMAC(LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_HELLO);

	}

	void PrepareSendChallenge()
	{
		PrepareBasePacketMAC(LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_REPLY);
	}
};
#endif

