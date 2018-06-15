// LoLaConnectionService.h

#ifndef _LOLACONNECTIONSERVICE_h
#define _LOLACONNECTIONSERVICE_h

#include <Services\IPacketSendService.h>
#include <Services\LatencyLoLaService.h>
#include <Services\LoLaServicesManager.h>
#include <LoLaLinkInfo.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <Callback.h>
#include <Crypto\TinyCRC.h>

#define PACKET_DEFINITION_CONNECTION_PAYLOAD_SIZE 5  //1 byte Sub-header + 4 byte pseudo-MAC +4 bytes for uint32_t
#define LOLA_CONNECTION_SERVICE_POLL_PERIOD_MILLIS 1000

#define LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD 3000
#define CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING 2000
#define CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_SLEEP 30000
#define CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_DISCONNECT (CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING*3 + 1000)



#define LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_BROADCAST	0x01
#define LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_REPLY		0x02
#define LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED	0x03
#define LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_HELLO		0x04
//#define LOLA_CONNECTION_SERVICE_SUBHEADER_

//TODO: Replace with one time random number.
#define LOLA_CONNECTION_HOST_PMAC 0x0E0F
#define LOLA_CONNECTION_REMOTE_PMAC 0x1A1B

class LoLaConnectionService : public IPacketSendService
{
private:
	uint32_t StartTime = 0;



protected:
	class LoLaPacketConnection : public ILoLaPacket
	{
	private:
		uint8_t Data[PACKET_DEFINITION_CONNECTION_PAYLOAD_SIZE];

	protected:
		uint8_t * GetRaw()
		{
			return &Data[LOLA_PACKET_HEADER_INDEX];
		}
	};
	bool LogSend = false;
	class ConnectionPacketDefinition : public PacketDefinition
	{
	public:
		uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID; }
		uint8_t GetHeader() { return PACKET_DEFINITION_CONNECTION_HEADER; }
		uint8_t GetPayloadSize() { return PACKET_DEFINITION_CONNECTION_PAYLOAD_SIZE; }
	} ConnectionDefinition;

	uint32_t LinkPMAC = 0;
	uint32_t RemotePMAC = 0;

	uint32_t TimeHelper = 0;
	uint8_t ConnectingState = 0;

	//TinyCrc CalculatorCRC;
	LoLaLinkInfo ConnectionInfo;

	LoLaPacketConnection PacketHolder;//Optimized memory usage grunt packet.

	//Subservices.
	LatencyLoLaService LatencyService;

	uint8_t SessionId = 0;


	//Callback handler
	Signal<const bool> ConnectionStatusUpdated;

public:
	LoLaConnectionService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_CONNECTION_SERVICE_POLL_PERIOD_MILLIS, loLa, &PacketHolder)
		, LatencyService(scheduler, loLa)
	{
		ConnectionInfo.SetDriver(loLa);
		ConnectionInfo.State = LoLaLinkInfo::ConnectionState::Disabled;
		AttachCallbacks();
		StartTime = Millis();
		RemotePMAC = 0;
	}

	bool AddSubServices(LoLaServicesManager * servicesManager)
	{
		if (!IPacketSendService::OnSetup())
		{
			return false;
		}

		if (servicesManager == nullptr || !servicesManager->Add(&LatencyService))
		{
			return false;
		}

		SetNextRunDefault();
		return true;
	}

	bool OnEnable()
	{
		ConnectionInfo.State = LoLaLinkInfo::ConnectionState::Setup;
		SetNextRunASAP();
		return true;
	}

	uint32_t GetElapsedSinceStart()
	{
		return Millis() - StartTime;
	}

	void StartTimeReset()
	{
		StartTime = Millis();
	}

	void OnDisable()
	{
	}

	void OnLatencyMeasurementCompleteInternal(const bool success)
	{
		OnLatencyMeasurementComplete(success);
	}

	LoLaLinkInfo* GetConnectionInfo()
	{
		return &ConnectionInfo;
	}

private:
	virtual void OnLatencyMeasurementComplete(const bool success) {}

private:
	void AttachCallbacks()
	{
		MethodSlot<LoLaConnectionService, const bool> memFunSlot(this, &LoLaConnectionService::OnLatencyMeasurementCompleteInternal);
		LatencyService.SetMeasurementCompleteCallback(memFunSlot);
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection service"));
	}
#endif // DEBUG_LOLA

protected:
	virtual void OnReceivedBroadcast(const uint8_t sessionId, uint8_t* data) {}
	virtual void OnChallengeReply(uint8_t* data) {}
	virtual void OnChallengeAccepted(uint8_t* data) {}
	virtual void OnHelloReceived(uint8_t* data) {}

	virtual void OnConnectionSetup() {}
	virtual void OnAwaitingConnection() {}
	virtual void OnConnecting() {}

protected:
	void PromoteToConnected()
	{
		//Connection ready, notify waiting services.
		if (ConnectionInfo.State != LoLaLinkInfo::ConnectionState::Connected)
		{
			ConnectionStatusUpdated.fire(true);
		}

		ConnectionInfo.State = LoLaLinkInfo::ConnectionState::Connected;
	}

	void PromoteToConnecting()
	{
		//Connecting, notify waiting services.
		if (ConnectionInfo.State != LoLaLinkInfo::ConnectionState::Connecting)
		{
			ConnectionStatusUpdated.fire(true);
		}

		ConnectionInfo.State = LoLaLinkInfo::ConnectionState::Connecting;
	}

	void DemoteToAwaiting()
	{
		//Awaiting connection, notify waiting services.
		if (ConnectionInfo.State != LoLaLinkInfo::ConnectionState::AwaitingConnection)
		{
			ConnectionStatusUpdated.fire(true);
		}

		ConnectionInfo.State = LoLaLinkInfo::ConnectionState::AwaitingConnection;
	}


	void DemoteToDisconnected()
	{
		//Connection lost, notify waiting services.
		ConnectionStatusUpdated.fire(false);

		ConnectionInfo.State = LoLaLinkInfo::ConnectionState::Setup;
	}

	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		return packetMap->AddMapping(&ConnectionDefinition);
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		if (header == PACKET_DEFINITION_CONNECTION_HEADER)
		{
#ifdef DEBUG_LOLA
			Serial.print(F("Packet: "));
			Serial.println(incomingPacket->GetPayload()[0]);
#endif
			if (ShouldProcessPackets())
			{
				switch (incomingPacket->GetPayload()[0])
				{
				case LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_BROADCAST:
					OnReceivedBroadcast(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
					break;
				case LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_REPLY:
					OnChallengeReply(&incomingPacket->GetPayload()[1]);
					break;
				case LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED:
					OnChallengeAccepted(&incomingPacket->GetPayload()[1]);
					break;
				case LOLA_CONNECTION_SERVICE_SUBHEADER_CHALLENGE_HELLO:
					OnHelloReceived(&incomingPacket->GetPayload()[1]);
					break;
				default:
					break;
				}
			}
			else
			{
#ifdef DEBUG_LOLA
				Serial.println(F("Denied: "));
#endif
			}

			return true;
		}
		return false;
	}

	bool ProcessAck(const uint8_t header, const uint8_t id)
	{
		if (header == PACKET_DEFINITION_CONNECTION_HEADER)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	bool OnSetup()
	{
		if (IPacketSendService::OnSetup())
		{
			SetNextRunDefault();
			SessionId = 0;
			return true;
		}
		return false;
	}

	void OnSendOk()
	{
#ifdef DEBUG_LOLA
		if (LogSend)
			Serial.println(F("OnSendOk: "));
#endif
	}

	void OnSendDelayed()
	{
#ifdef DEBUG_LOLA
		if (LogSend)
			Serial.println(F("OnSendDelayed: "));
#endif
	}

	void OnSendRetrying()
	{
#ifdef DEBUG_LOLA
		if (LogSend)
			Serial.println(F("OnSendRetrying: "));
#endif
	}

	void OnSendFailed()
	{
#ifdef DEBUG_LOLA
		if (LogSend)
			Serial.println(F("OnSendFailed: "));
#endif
	}

	void OnSendTimedOut()
	{
#ifdef DEBUG_LOLA
		if (LogSend)
			Serial.println(F("OnSendTimedOut: "));
#endif
	}


	virtual bool ShouldProcessPackets()
	{
		return (SessionId != 0);
	}

	virtual void OnKeepingConnected(const uint32_t elapsedSinceLastReceived)
	{
#ifndef MOCK_RADIO
		if (elapsedSinceLastReceived > CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_DISCONNECT)
		{
#ifdef DEBUG_LOLA
			Serial.print(F("Bi directional commms lost over "));
			Serial.print(CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_DISCONNECT / 1000);
			Serial.println(F(" seconds."));
#endif
			DemoteToDisconnected();
			SetNextRunASAP();
		}
		else if (elapsedSinceLastReceived > CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING)
		{
#ifdef DEBUG_LOLA
			Serial.println(F("Keep alive Ping"));
#endif
			//LatencyService.RequestSinglePing();
			SetNextRunDelay(CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING);
		}
		else
#endif // !MOCK_RADIO
		{
			SetNextRunDefault();
		}
	}

	void OnTookTooLong()
	{
		ConnectionInfo.State = LoLaLinkInfo::ConnectionState::Setup;
		SetNextRunDelay((uint32_t)CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING * 40);
	}

	void OnService()
	{
		switch (ConnectionInfo.State)
		{
		case LoLaLinkInfo::ConnectionState::Setup:
			StartTime = Millis();
			SetNextRunASAP();
			OnConnectionSetup();
			ConnectionInfo.State = LoLaLinkInfo::ConnectionState::AwaitingConnection;
			break;
		case LoLaLinkInfo::ConnectionState::AwaitingConnection:
			SetNextRunDefault();
			OnAwaitingConnection();
			break;
		case LoLaLinkInfo::ConnectionState::Connecting:
			SetNextRunASAP();
			OnConnecting();
			break;
		case LoLaLinkInfo::ConnectionState::Connected:
			SetNextRunDelay(3000);
			OnKeepingConnected(Millis() - GetLoLa()->GetLastReceivedMillis());
			break;
		case LoLaLinkInfo::ConnectionState::Disabled:
#ifdef DEBUG_LOLA
			Serial.println(F("LoLaConnectionState::Disabled"));
#endif
		default:
			Disable();
			return;
		}
	}
};
#endif