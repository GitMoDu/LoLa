// LoLaConnectionHostService.h

#ifndef _LOLACONNECTIONREMOTESERVICE_h
#define _LOLACONNECTIONREMOTESERVICE_h

#include <Services\Connection\LoLaConnectionService.h>

class LoLaConnectionRemoteService : public LoLaConnectionService
{
private:
	enum RemoteStateAwaitingConnectionEnum
	{
		SearchingForBroadcast = 0,
		GotBroadcast = 1,
		SendingChallenge = 2,
		AwaitingCallengeResponse = 3,
		ResponseOk = 4,
		ResponseNotOk = 5
	};

	enum RemoteStateConnectingEnum
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
		ConnectingState = RemoteStateAwaitingConnectionEnum::SearchingForBroadcast;
		SessionId = 0;
	}

	bool ShouldProcessPackets()
	{
		if (!LoLaConnectionService::ShouldProcessPackets())
		{
			return ConnectingState == RemoteStateAwaitingConnectionEnum::SearchingForBroadcast;
		}
		return true;
	}

	void OnReceivedBroadcast(const uint8_t sessionId, uint8_t* data)
	{
		if (ConnectionInfo.State == LoLaLinkInfo::ConnectionState::Connected ||
			ConnectionInfo.State == LoLaLinkInfo::ConnectionState::Connecting)
		{
			if (RemotePMAC == ((data[0] & 0x00FF) + ((uint32_t)data[1] << 8) + ((uint32_t)data[2] << 16) + ((uint32_t)data[3] << 24)))
			{
				DemoteToDisconnected();
				SetNextRunASAP();
			}
		}
		else if (ConnectionInfo.State == LoLaLinkInfo::ConnectionState::AwaitingConnection)
		{
			//TODO: Event warning of new Host to connect.
			RemotePMAC = (uint32_t)data[0] & 0x00FF;
			RemotePMAC += ((uint32_t)data[1]) << 8;
			RemotePMAC += ((uint32_t)data[2]) << 16;
			RemotePMAC += ((uint32_t)data[3]) << 24;

			SessionId = sessionId;

			ConnectingState = RemoteStateAwaitingConnectionEnum::GotBroadcast;
			SetNextRunASAP();
		}
		else
		{
			//Ignore, nothing to do.
		}
	}

	void OnChallengeAccepted(uint8_t* data)
	{
#ifdef DEBUG_LOLA
		Serial.println(F("OnChallengeAccepted: "));
#endif
		if (ConnectingState == RemoteStateAwaitingConnectionEnum::AwaitingCallengeResponse)
		{
#ifdef DEBUG_LOLA
			Serial.println(F("ResponseOk: "));
#endif
			ConnectingState = RemoteStateAwaitingConnectionEnum::ResponseOk;

		}

		//RemoteState = RemoteStateAwaitingConnectionEnum::ResponseNotOk;
	}

	void OnConnecting()
	{
		switch (ConnectingState)
		{
		case RemoteStateConnectingEnum::Starting:
			TimeHelper = Millis();
			if (SessionId == 0)
			{
				DemoteToAwaiting();
				ConnectingState = RemoteStateAwaitingConnectionEnum::SearchingForBroadcast;
				SetNextRunASAP();
				return;
			}
			break;
		case RemoteStateConnectingEnum::Diagnostics:
			if (SessionId == 0)
			{
				DemoteToAwaiting();
				ConnectingState = RemoteStateAwaitingConnectionEnum::SearchingForBroadcast;
				SetNextRunASAP();
				return;
			}
#ifdef DEBUG_LOLA
			Serial.print(F("Connecting: "));
			Serial.println(SessionId);
#endif
			//LatencyService.RequestRefreshPing();
			SetNextRunDelay(LOLA_LATENCY_SERVICE_UNABLE_TO_COMMUNICATE_TIMEOUT_MILLIS + 1000);
			ConnectingState = RemoteStateConnectingEnum::MeasuringLatency;
			break;
		case RemoteStateConnectingEnum::MeasuringLatency:
			ConnectingState = RemoteStateConnectingEnum::ConnectionEscalationFailed;
			SetNextRunASAP();
			break;
		case RemoteStateConnectingEnum::ConnectionGreenLight:
			PromoteToConnected();
			SetNextRunASAP();
			break;
		case RemoteStateConnectingEnum::ConnectionEscalationFailed:
		default:
			DemoteToAwaiting();
			ConnectingState = RemoteStateAwaitingConnectionEnum::SearchingForBroadcast;
			SetNextRunASAP();
			break;
		}
	}

	uint32_t LastHelloSent = 0;
	void OnAwaitingConnection()
	{
		switch (ConnectingState)
		{
		case RemoteStateAwaitingConnectionEnum::SearchingForBroadcast:
			if (Millis() - LastHelloSent > CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING)
			{
#ifdef DEBUG_LOLA
				Serial.println(F("Sending Hello."));
#endif
				PrepareHello();
				RequestSendPacket();
				LastHelloSent = Millis();
			}
			else if (GetElapsedSinceStart() > CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
			{
				OnTookTooLong();
				return;
			}
			else
			{
				SetNextRunDelay(LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD);
			}
			break;
		case RemoteStateAwaitingConnectionEnum::GotBroadcast:
			if (SessionId == 0)
			{
				ConnectingState = RemoteStateAwaitingConnectionEnum::SearchingForBroadcast;
				SetNextRunDefault();
				return;
			}
#ifdef DEBUG_LOLA
			Serial.print(F("GotBroadcast, sending challenge: "));
			Serial.println(SessionId);
#endif
			PrepareSendChallenge();
			RequestSendPacket();
			ConnectingState = RemoteStateAwaitingConnectionEnum::SendingChallenge;
			//LogSend = true;
			break;
		case RemoteStateAwaitingConnectionEnum::SendingChallenge:
			LogSend = false;
			ConnectingState = RemoteStateAwaitingConnectionEnum::AwaitingCallengeResponse;
			SetNextRunDelay(LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD);
			break;
		case RemoteStateAwaitingConnectionEnum::AwaitingCallengeResponse:
			ConnectingState = RemoteStateAwaitingConnectionEnum::SearchingForBroadcast;
			SetNextRunDefault();
#ifdef DEBUG_LOLA
			Serial.print(F("CallengeResponse timed out: "));
			Serial.println(SessionId);
#endif
			break;
		case RemoteStateAwaitingConnectionEnum::ResponseOk:
			PromoteToConnecting();
			ConnectingState = RemoteStateConnectingEnum::Starting;
			SetNextRunASAP();
			break;
		case RemoteStateAwaitingConnectionEnum::ResponseNotOk:
			ConnectingState = RemoteStateAwaitingConnectionEnum::SearchingForBroadcast;
			SetNextRunDelay(1000);
			break;
		default:
			break;
		}
	}

private:
	uint32_t ChallengeResponse = 0;

	void PrepareHello()
	{
		PacketHolder.SetDefinition(&ConnectionDefinition);
		PacketHolder.SetId(0);

		PacketHolder.GetPayload()[0] = LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_HELLO;
		PacketHolder.GetPayload()[1] = LinkPMAC & 0x00FF;
		PacketHolder.GetPayload()[2] = (LinkPMAC >> 8) & 0x00FF;
		PacketHolder.GetPayload()[3] = (LinkPMAC >> 16) & 0x00FF;
		PacketHolder.GetPayload()[4] = (LinkPMAC >> 24) & 0x00FF;
	}

	void PrepareSendChallenge()
	{
		PacketHolder.SetDefinition(&ConnectionDefinition);
		PacketHolder.SetId(SessionId);

		PacketHolder.GetPayload()[0] = LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_REPLY;
		PacketHolder.GetPayload()[1] = LinkPMAC & 0x00FF;
		PacketHolder.GetPayload()[2] = (LinkPMAC >> 8) & 0x00FF;
		PacketHolder.GetPayload()[3] = (LinkPMAC >> 16) & 0x00FF;
		PacketHolder.GetPayload()[4] = (LinkPMAC >> 24) & 0x00FF;
	}
};
#endif

