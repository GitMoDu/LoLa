// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h

#include <Services\IPacketSendService.h>
#include <Services\LatencyLoLaService.h>
#include <Services\LoLaServicesManager.h>
#include <LoLaLinkInfo.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <Callback.h>
#include <Crypto\TinyCRC.h>

#define LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE	5  //1 byte Sub-header + 4 byte pseudo-MAC or other uint32_t

#define LOLA_LINK_SERVICE_POLL_PERIOD_MILLIS				500
#define LOLA_LINK_SERVICE_BROADCAST_PERIOD					1000
#define LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP			30000
#define LOLA_LINK_SERVICE_SLEEP_PERIOD						60000


#define LOLA_LINK_SERVICE_MIN_ELAPSED_BEFORE_HELLO			500
#define LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD					(LOLA_LINK_SERVICE_MIN_ELAPSED_BEFORE_HELLO*4)
#define LOLA_LINK_SERVICE_PERIOD_INTERVENTION				(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*2)
#define LOLA_LINK_SERVICE_PANIC								(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*3)
#define LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_DISCONNECT		(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*4)

#define LOLA_LINK_SERVICE_SLOW_CHECK_PERIOD					LOLA_LINK_SERVICE_MIN_ELAPSED_BEFORE_HELLO
#define LOLA_LINK_SERVICE_FAST_CHECK_PERIOD			50

#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_BROADCAST	0x01
#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_REPLY		0x02
#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED		0x03
#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_HELLO		0x04

//TODO: Future.
//#define LOLA_CONNECTION_SERVICE_SUBHEADER_LINK_INFO				0X05
//#define LOLA_CONNECTION_SERVICE_SUBHEADER_NTP					0X06

//TODO: Replace with one time random number.
#define LOLA_LINK_HOST_PMAC 0x0E0F
#define LOLA_LINK_REMOTE_PMAC 0x1A1B

class LoLaLinkService : public IPacketSendService
{
private:
	uint32_t StateStartTime = 0;

protected:
	bool LogSend = false;
	class ConnectionPacketDefinition : public PacketDefinition
	{
	public:
		uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID; }
		uint8_t GetHeader() { return PACKET_DEFINITION_CONNECTION_HEADER; }
		uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE; }
	} ConnectionDefinition;

	enum ConnectingEnum
	{
		ConnectingStarting = 250,
		Diagnostics = 251,
		MeasuringLatency = 252,
		MeasurementLatencyDone = 253,
		ConnectionGreenLight = 254,
		ConnectionEscalationFailed = 255,
	};

	//Link session variables
	uint32_t LinkPMAC = 0;
	uint32_t RemotePMAC = 0;
	uint8_t SessionId = 0;

	uint8_t ConnectingState = 0;

	LoLaLinkInfo LinkInfo;

	LoLaPacketSlim PacketHolder;//Optimized memory usage grunt packet.

	//Subservices.
	LatencyLoLaService LatencyService;

	uint32_t TimeHelper = 0;

	//Callback handler
	Signal<const uint8_t> ConnectionStatusUpdated;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} ATUI;

public:
	LoLaLinkService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_POLL_PERIOD_MILLIS, loLa, &PacketHolder)
		, LatencyService(scheduler, loLa)
	{
		LinkInfo.SetDriver(loLa);
		LinkInfo.LinkState = LoLaLinkInfo::LinkStateEnum::Disabled;
		AttachCallbacks();
		StateStartTime = Millis();
		RemotePMAC = 0;
	}

	//Param is 
	//LoLaLinkInfo::LinkState
	void SetOnConnectionStatusUpdatedCallback(const Slot<const uint8_t>& slot)
	{
		ConnectionStatusUpdated.attach(slot);
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
		UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Setup);
		return true;
	}

	uint32_t GetElapsedSinceStart()
	{
		return Millis() - StateStartTime;
	}

	void StartTimeReset()
	{
		StateStartTime = Millis();
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
		return &LinkInfo;
	}

protected:
	virtual void OnBroadcastReceived(const uint8_t sessionId, uint8_t* data) {}
	virtual void OnChallengeReplyReceived(const uint8_t sessionId, uint8_t* data) {}
	virtual void OnChallengeAcceptedReceived(const uint8_t sessionId, uint8_t* data) {}
	virtual void OnHelloReceived(const uint8_t sessionId, uint8_t* data) {}

	virtual void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState) {}
	virtual void OnLinkWarningLow() {}
	virtual void OnLinkWarningMedium() {}
	virtual void OnLinkWarningHigh() {}

	virtual void OnAwaitingConnection() {}
	virtual void OnConnecting() {}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection service"));
	}
#endif // DEBUG_LOLA

	void UpdateLinkState(const LoLaLinkInfo::LinkStateEnum newState)
	{
		if (LinkInfo.LinkState != newState)
		{
			OnLinkStateChanged(newState);
			ConnectionStatusUpdated.fire(newState);

			if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connected)
			{
				LinkInfo.ResetLinkStarted();
				//TODO: Take to chance to do something when link is lost.
			}
			else if (newState == LoLaLinkInfo::LinkStateEnum::Connected)
			{
				LinkInfo.StampLinkStarted();
				//TODO: Take to chance to do something when link is established.
			}
		}
		LinkInfo.LinkState = newState;
	}

	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		return packetMap->AddMapping(&ConnectionDefinition);
	}

	void ClearSession()
	{
		SessionId = 0;
		RemotePMAC = 0;
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		if (header == PACKET_DEFINITION_CONNECTION_HEADER)
		{
			if (ShouldProcessPackets())
			{
				switch (incomingPacket->GetPayload()[0])
				{
				case LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_BROADCAST:
					OnBroadcastReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
					break;
				case LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_REPLY:
					OnChallengeReplyReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
					break;
				case LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED:
					OnChallengeAcceptedReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
					break;
				case LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_HELLO:
					OnHelloReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
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
			ClearSession();
			return true;
		}
		return false;
	}

	void PrepareHello()
	{
		PrepareBasePacketMAC(LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_HELLO);
	}

	virtual bool ShouldProcessPackets()
	{
		if (SessionId != 0 ||
			LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::AwaitingConnection ||
			LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::AwaitingSleeping)
		{
			return true;
		}

		return false;
	}

	void OnKeepingConnected(const uint32_t elapsedSinceLastReceived)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connected)
		{
			if (elapsedSinceLastReceived > LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_DISCONNECT)
			{
#ifdef DEBUG_LOLA
				Serial.print(F("Bi directional commms lost over "));
				Serial.print(LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_DISCONNECT / 1000);
				Serial.println(F(" seconds."));
#endif
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingConnection);
				SetNextRunASAP();
			}
			else if (elapsedSinceLastReceived > LOLA_LINK_SERVICE_PANIC)
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
				OnLinkWarningHigh();
			}
			else if (elapsedSinceLastReceived > LOLA_LINK_SERVICE_PERIOD_INTERVENTION)
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
				OnLinkWarningMedium();
			}
			else if (elapsedSinceLastReceived > LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD)
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_SLOW_CHECK_PERIOD);
				OnLinkWarningLow();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_MIN_ELAPSED_BEFORE_HELLO);
			}
		}
		else
		{
			SetNextRunASAP();
		}
	}

	void OnService()
	{
		switch (LinkInfo.LinkState)
		{
		case LoLaLinkInfo::LinkStateEnum::Setup:
			StartTimeReset();
			SetNextRunASAP();
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingConnection);
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingConnection:
			SetNextRunASAP();
			OnAwaitingConnection();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			StartTimeReset();
			SetNextRunASAP();
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingConnection);
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
			SetNextRunASAP();
			OnConnecting();
			break;
		case LoLaLinkInfo::LinkStateEnum::Connected:
			SetNextRunASAP();
			OnKeepingConnected(Millis() - GetLoLa()->GetLastReceivedMillis());
			break;
		case LoLaLinkInfo::LinkStateEnum::Disabled:
		default:
			Disable();
			return;
		}
	}

	void PrepareBasePacket(const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&ConnectionDefinition);
		PacketHolder.SetId(SessionId);
		PacketHolder.GetPayload()[0] = subHeader;
	}

	void PrepareBasePacketMAC(const uint8_t subHeader)
	{
		PrepareBasePacket(subHeader);

		ATUI.uint = LinkPMAC;
		PacketHolder.GetPayload()[1] = ATUI.array[0];
		PacketHolder.GetPayload()[2] = ATUI.array[1];
		PacketHolder.GetPayload()[3] = ATUI.array[2];
		PacketHolder.GetPayload()[4] = ATUI.array[3];
	}

private:
	void OnLatencyMeasurementComplete(const bool success)
	{
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connecting &&
			ConnectingState == ConnectingEnum::MeasuringLatency)
		{
			if (success)
			{
				LinkInfo.SetRTT(LatencyService.GetRTT());
				ConnectingState = ConnectingEnum::MeasurementLatencyDone;
			}
			else
			{
				LinkInfo.ResetLatency();
			}
			SetNextRunASAP();
		}
	}

	void AttachCallbacks()
	{
		MethodSlot<LoLaLinkService, const bool> memFunSlot(this, &LoLaLinkService::OnLatencyMeasurementCompleteInternal);
		LatencyService.SetMeasurementCompleteCallback(memFunSlot);
	}
};
#endif