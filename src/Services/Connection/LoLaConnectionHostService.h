// LoLaConnectionHostService.h

#ifndef _LOLACONNECTIONHOSTSERVICE_h
#define _LOLACONNECTIONHOSTSERVICE_h

#include <Services\Connection\LoLaConnectionService.h>

class LoLaConnectionHostService : public LoLaConnectionService
{
private:
	enum HostStateAwaitingConnectionEnum
	{
		Starting = 0,
		BroadcastingOpenSession = 1,
		SentBroadCast = 2,
		GotResponse = 3,
		ResponseOk = 4,
		SendingResponseOk = 5,
		ResponseNotOk = 6,
	};

	enum HostStateConnectingEnum
	{
		ConnectingStarting = 7,
		Diagnostics = 8,
		MeasuringLatency = 9,
		MeasurementLatencyDone = 10,
		ConnectionGreenLight = 11,
		ConnectionEscalationFailed = 12
	};

public:
	LoLaConnectionHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaConnectionService(scheduler, loLa)
	{
		LinkPMAC = LOLA_CONNECTION_HOST_PMAC;
		ConnectingState = HostStateAwaitingConnectionEnum::Starting;
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
		if (ConnectingState == HostStateAwaitingConnectionEnum::SentBroadCast ||
			ConnectingState == HostStateAwaitingConnectionEnum::BroadcastingOpenSession)
		{
			RemotePMAC = (uint32_t)data[0] & 0x00FF;
			RemotePMAC += ((uint32_t)data[1]) << 8;
			RemotePMAC += ((uint32_t)data[2]) << 16;
			RemotePMAC += ((uint32_t)data[3]) << 24;

			ConnectingState = HostStateAwaitingConnectionEnum::GotResponse;
			SetNextRunASAP();
		}
	}

	void OnHelloReceived(uint8_t* data)
	{
		if (ConnectingState == HostStateAwaitingConnectionEnum::SentBroadCast ||
			ConnectingState == HostStateAwaitingConnectionEnum::BroadcastingOpenSession)
		{
			Enable();
			SetNextRunASAP();
		}
	}

	void OnConnecting()
	{
		switch (ConnectingState)
		{
		case HostStateConnectingEnum::ConnectingStarting:
			TimeHelper = Millis();
#ifdef DEBUG_LOLA
			Serial.println(F("OnConnecting"));
#endif
			if (SessionId == 0)
			{
				DemoteToAwaiting();
				ConnectingState = HostStateAwaitingConnectionEnum::BroadcastingOpenSession;
			}
			else
			{
				ConnectingState = HostStateConnectingEnum::Diagnostics;
			}
			SetNextRunASAP();
			break;
		case HostStateConnectingEnum::Diagnostics:
#ifdef DEBUG_LOLA
			Serial.println(F("StartingDiagnostics"));
#endif
			ConnectingState = HostStateConnectingEnum::MeasuringLatency;
			LatencyService.RequestRefreshPing();
			SetNextRunDelay(LOLA_LATENCY_SERVICE_UNABLE_TO_COMMUNICATE_TIMEOUT_MILLIS + 1000);
		case HostStateConnectingEnum::MeasuringLatency:
			ConnectingState = HostStateConnectingEnum::ConnectionEscalationFailed;
			SetNextRunASAP();
			break;
		case HostStateConnectingEnum::MeasurementLatencyDone:
			break;
		case HostStateConnectingEnum::ConnectionGreenLight:
			PromoteToConnected();
			SetNextRunASAP();
			break;
		case HostStateConnectingEnum::ConnectionEscalationFailed:
		default:
			ConnectingState = HostStateAwaitingConnectionEnum::BroadcastingOpenSession;
			OnTookTooLong();
			SetNextRunASAP();
			break;
		}
	}

	void OnAwaitingConnection()
	{
		switch (ConnectingState)
		{
		case HostStateAwaitingConnectionEnum::Starting:
			TimeHelper = Millis();
			ConnectingState = HostStateAwaitingConnectionEnum::BroadcastingOpenSession;
			SetNextRunASAP();
			break;
		case HostStateAwaitingConnectionEnum::BroadcastingOpenSession:
			if (Millis() - TimeHelper > LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD)
			{
#ifdef DEBUG_LOLA
				Serial.print(F("B: "));
				Serial.println(SessionId);
#endif
				PreparePacketBroadcast();
				RequestSendPacket();
				ConnectingState = HostStateAwaitingConnectionEnum::SentBroadCast;
			}
			else if (GetElapsedSinceStart() > CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
			{
				OnTookTooLong();
			}
			else
			{
				SetNextRunDelay(LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD);
			}
			break;
		case HostStateAwaitingConnectionEnum::SentBroadCast:
			ConnectingState = HostStateAwaitingConnectionEnum::BroadcastingOpenSession;
			SetNextRunDelay(LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD);
			break;
		case HostStateAwaitingConnectionEnum::GotResponse:
#ifdef DEBUG_LOLA
			Serial.print(F("GotResponse: "));
			Serial.println(SessionId);
#endif
			if (true || RemotePMAC != 0)
			{
				ConnectingState = HostStateAwaitingConnectionEnum::ResponseOk;
			}
			else
			{
#ifdef DEBUG_LOLA
				Serial.println(F("ResponseNotOk"));
#endif
				ConnectingState = HostStateAwaitingConnectionEnum::ResponseNotOk;
			}
			SetNextRunASAP();

			break;
		case HostStateAwaitingConnectionEnum::ResponseOk:
#ifdef DEBUG_LOLA
			Serial.println(F("ResponseOk:"));
#endif
			PrepareResponseOk();
			RequestSendPacket();
			ConnectingState = HostStateAwaitingConnectionEnum::SendingResponseOk;
			SetNextRunASAP();
			break;
		case HostStateAwaitingConnectionEnum::ResponseNotOk:
			ConnectingState = HostStateAwaitingConnectionEnum::Starting;
			SetNextRunDefault();
			break;
		case HostStateAwaitingConnectionEnum::SendingResponseOk:
			SetNextRunASAP();
			ConnectingState = HostStateConnectingEnum::ConnectingStarting;
			PromoteToConnecting();
			break;
		default:
			ConnectingState = HostStateAwaitingConnectionEnum::ResponseNotOk;
			SetNextRunASAP();
			break;
		}
	}

	void OnConnectionSetup()
	{
		NewSessionId();
		RemotePMAC = 0;
		SetNextRunDefault();
		ConnectingState = HostStateAwaitingConnectionEnum::Starting;
	}

	void OnKeepingConnected(const uint32_t elapsedSinceLastReceived)
	{
		LoLaConnectionService::OnKeepingConnected(elapsedSinceLastReceived);
	}

private:
	void OnLatencyMeasurementComplete(const bool success)
	{
		if (ConnectingState == HostStateConnectingEnum::MeasuringLatency)
		{
			if (success)
			{
				ConnectingState = HostStateConnectingEnum::MeasurementLatencyDone;
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
		PacketHolder.SetDefinition(&ConnectionDefinition);
		PacketHolder.SetId(SessionId);

		PacketHolder.GetPayload()[0] = LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_BROADCAST;
		PacketHolder.GetPayload()[1] = LinkPMAC & 0x00FF;
		PacketHolder.GetPayload()[2] = (LinkPMAC >> 8) & 0x00FF;
		PacketHolder.GetPayload()[3] = (LinkPMAC >> 16) & 0x00FF;
		PacketHolder.GetPayload()[4] = (LinkPMAC >> 24) & 0x00FF;
	}

	void PrepareResponseOk()
	{
		PacketHolder.SetDefinition(&ConnectionDefinition);
		PacketHolder.SetId(SessionId);

		PacketHolder.GetPayload()[0] = LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED;
		PacketHolder.GetPayload()[1] = LinkPMAC & 0x00FF;
		PacketHolder.GetPayload()[2] = (LinkPMAC >> 8) & 0x00FF;
		PacketHolder.GetPayload()[3] = (LinkPMAC >> 16) & 0x00FF;
		PacketHolder.GetPayload()[4] = (LinkPMAC >> 24) & 0x00FF;
	}
};
#endif