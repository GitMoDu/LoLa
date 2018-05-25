// LoLaConnectionService.h

#ifndef _LOLACONNECTIONSERVICE_h
#define _LOLACONNECTIONSERVICE_h

#include <Services\IPacketSendService.h>
#include <Services\LatencyLoLaService.h>
#include <Services\LoLaServicesManager.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <Callback.h>

#define PACKET_DEFINITION_CONNECTION_PAYLOAD_SIZE 4
#define LOLA_CONNECTION_SERVICE_POLL_PERIOD_MILLIS 1000

#define LOLA_CONNECTION_SERVICE_BROADCAST_PERIOD 1000


class LoLaConnectionService : public IPacketSendService
{
protected:
	class ConnectionPacketDefinition : public PacketDefinition
	{
	public:
		uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID; }
		uint8_t GetHeader() { return PACKET_DEFINITION_CONNECTION_HEADER; }
		uint8_t GetPayloadSize() { return PACKET_DEFINITION_CONNECTION_PAYLOAD_SIZE; }
	} ConnectionDefinition;

	enum LoLaConnectionState
	{
		Disabled,
		Setup,
		AwaitingConnection,
		Connected
	} ConnectionState = Disabled;


	LoLaPacketSlim PacketHolder;//Optimized memory usage grunt packet.

	//Subservices.
	LatencyLoLaService LatencyService;

	uint8_t SessionId = 0;
	uint32_t StartTime = 0;

	//Callback handler
	Signal<const bool> ConnectionStatusUpdated;

public:
	LoLaConnectionService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_CONNECTION_SERVICE_POLL_PERIOD_MILLIS, loLa, &PacketHolder)
		, LatencyService(scheduler, loLa)
	{
		AttachCallbacks();
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
		ConnectionState = LoLaConnectionState::Setup;
		return true;
	}

	void OnDisable()
	{
	}

	void OnLatencyMeasurementCompleteInternal(const bool success)
	{
		OnLatencyMeasurementComplete(success);
	}

private:
	virtual void OnLatencyMeasurementComplete(const bool success) {}

private:
	void AttachCallbacks()
	{
		MethodSlot<LoLaConnectionService, const bool> memFunSlot(this, &LoLaConnectionService::OnLatencyMeasurementCompleteInternal);
		LatencyService.SetMeasurementCompleteCallback(memFunSlot);
	}

	bool ShouldProcessPackets()
	{
		return (SessionId != 0);
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection service"));
	}
#endif // DEBUG_LOLA

	void PromoteToConnected()
	{
		//Connection ready, notify waiting services.
		if (ConnectionState != LoLaConnectionState::Connected)
		{
			ConnectionStatusUpdated.fire(true);
		}

		ConnectionState = LoLaConnectionState::Connected;
	}

	void DemoteToDisconnected()
	{
		//Connection lost, notify waiting services.
		if (ConnectionState == LoLaConnectionState::Connected)
		{
			ConnectionStatusUpdated.fire(false);
		}

		ConnectionState = LoLaConnectionState::Setup;
	}

	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		return packetMap->AddMapping(&ConnectionDefinition);
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		if (header == PACKET_DEFINITION_CONNECTION_HEADER)
		{
			if (ShouldProcessPackets())
			{

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
	}

	void OnSendDelayed()
	{
	}

	void OnSendRetrying()
	{
	}

	void OnSendFailed()
	{
	}

	void OnSendTimedOut()
	{
	}

	virtual void OnConnectionSetup()
	{

	}

	virtual void OnAwaitingConnection()
	{

	}

#define CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING 2000
#define CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_DISCONNECT (CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING*3 + 1000)
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
			LatencyService.RequestSinglePing();
			SetNextRunDelay(CONNECTION_SERVICE_MAX_ELAPSED_BEFORE_PING);
		}
		else
#endif // !MOCK_RADIO
		{
			SetNextRunDefault();
		}
	}

	void OnService()
	{
		switch (ConnectionState)
		{
		case LoLaConnectionState::Setup:
			SetNextRunASAP();
			OnConnectionSetup();
			ConnectionState = LoLaConnectionState::AwaitingConnection;
			break;
		case LoLaConnectionState::AwaitingConnection:
			OnAwaitingConnection();
			break;
		case LoLaConnectionState::Connected:
			SetNextRunDelay(3000);
			OnKeepingConnected(Millis() - GetLoLa()->GetLastReceivedMillis());
			break;
		case LoLaConnectionState::Disabled:
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