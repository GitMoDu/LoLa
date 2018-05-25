// LoLaConnectionHostService.h

#ifndef _LOLACONNECTIONHOSTSERVICE_h
#define _LOLACONNECTIONHOSTSERVICE_h

#include <Services\Connection\LoLaConnectionService.h>

class LoLaConnectionHostService : public LoLaConnectionService
{
private:
	enum HostStateAwaitingConnectionEnum
	{
		BroadcastingOpenSession,
		SentBroadCast,
		GotResponse,
		ResponseOk,
		ResponseNotOk,
		StartingDiagnostics,
		MeasuringLatency,
		MeasuringEtc,
		ConnectionGreenLight,
		ConnectionEscalationFailed,
	} HostState = BroadcastingOpenSession;


public:
	LoLaConnectionHostService(Scheduler *scheduler, ILoLa* loLa)
		: LoLaConnectionService(scheduler, loLa)
	{

	}
protected:
	void OnAwaitingConnection()
	{
		switch (HostState)
		{
		case HostStateAwaitingConnectionEnum::BroadcastingOpenSession:
			PreparePacketBroadcast();
			RequestSendPacket();
			HostState = HostStateAwaitingConnectionEnum::SentBroadCast;
			HostState = HostStateAwaitingConnectionEnum::StartingDiagnostics;//Skip connection escalation
			break;
		case HostStateAwaitingConnectionEnum::SentBroadCast:
			SetNextRunDelay(LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD);
			break;
		case HostStateAwaitingConnectionEnum::GotResponse:
			SetNextRunASAP();
			break;
		case HostStateAwaitingConnectionEnum::ResponseOk:
			SetNextRunASAP();
			break;
		case HostStateAwaitingConnectionEnum::ResponseNotOk:
			HostState = HostStateAwaitingConnectionEnum::SentBroadCast;
			SetNextRunDefault();
			break;
		case HostStateAwaitingConnectionEnum::StartingDiagnostics:
			Serial.println(F("RequestRefreshPing"));
			HostState = HostStateAwaitingConnectionEnum::MeasuringLatency;
			LatencyService.RequestRefreshPing();
			StartTime = Millis();
			SetNextRunDelay(LOLA_LATENCY_SERVICE_UNABLE_TO_COMMUNICATE_TIMEOUT_MILLIS + 1000);
			break;
		case HostStateAwaitingConnectionEnum::MeasuringLatency:
			HostState = HostStateAwaitingConnectionEnum::ConnectionEscalationFailed;
			SetNextRunASAP();
			break;
		case HostStateAwaitingConnectionEnum::MeasuringEtc:
			HostState = HostStateAwaitingConnectionEnum::ConnectionGreenLight;
			break;
		case HostStateAwaitingConnectionEnum::ConnectionGreenLight:
			PromoteToConnected();
			SetNextRunASAP();
			break;
		case HostStateAwaitingConnectionEnum::ConnectionEscalationFailed:
			Serial.println(F("Connection Failed"));
			HostState = HostStateAwaitingConnectionEnum::BroadcastingOpenSession;
			SetNextRunDelay(5000);
			break;
		default:
			HostState = HostStateAwaitingConnectionEnum::StartingDiagnostics;
			break;
		}
	}

	virtual void OnConnectionSetup()
	{
		NewSessionId();
		HostState = HostStateAwaitingConnectionEnum::BroadcastingOpenSession;
	}

	void OnKeepingConnected(const uint32_t elapsedSinceLastReceived)
	{
		LoLaConnectionService::OnKeepingConnected(elapsedSinceLastReceived);
		//DemoteToDisconnected();
	}

private:

	void OnLatencyMeasurementComplete(const bool success) 
	{
		if (HostState == HostStateAwaitingConnectionEnum::MeasuringLatency)
		{
			if (success)
			{
				HostState = HostStateAwaitingConnectionEnum::MeasuringEtc;
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
		//TODO: Put challenge info on payload
		PacketHolder.GetPayload()[0] = 1;
	}
};

#endif

