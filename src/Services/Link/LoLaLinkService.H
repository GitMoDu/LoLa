// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h

//#define LOLA_LINK_DIAGNOSTICS_ENABLED


#include <Services\IPacketSendService.h>
#include <Services\LoLaServicesManager.h>
#include <LoLaLinkInfo.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <Callback.h>
#include <Crypto\TinyCRC.h>
#include <Services\LoLaDiagnosticsService\LoLaDiagnosticsService.h>

#include <ClockSource.h>

#define LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE	5  //1 byte Sub-header + 4 byte pseudo-MAC or other uint32_t

#define LOLA_LINK_SERVICE_POLL_PERIOD_MILLIS				500
#define LOLA_LINK_SERVICE_BROADCAST_PERIOD					1000
#define LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP			30000
#define LOLA_LINK_SERVICE_SLEEP_PERIOD						60000


#define LOLA_LINK_SERVICE_MIN_ELAPSED_BEFORE_HELLO			500
#define LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD					(LOLA_LINK_SERVICE_MIN_ELAPSED_BEFORE_HELLO*2)
#define LOLA_LINK_SERVICE_PERIOD_INTERVENTION				(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*2)
#define LOLA_LINK_SERVICE_PANIC								(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*3)
#define LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT				(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*4)
#define LOLA_LINK_SERVICE_MAX_BEFORE_CONNECTING_CANCEL		(LOLA_LINK_SERVICE_BROADCAST_PERIOD*2)
#define LOLA_LINK_SERVICE_CONNECTING_MIN_DURATION			50

#define LOLA_LINK_SERVICE_SLOW_CHECK_PERIOD					LOLA_LINK_SERVICE_MIN_ELAPSED_BEFORE_HELLO
#define LOLA_LINK_SERVICE_FAST_CHECK_PERIOD					50

#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_BROADCAST		0x01
#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_REPLY			0x02
#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED		0x03

#define LOLA_LINK_SERVICE_ACK_SUBHEADER_HELLO				0x00
#define LOLA_LINK_SERVICE_ACK_SUBHEADER_NTP					0X01

//TODO: Future.
//#define LOLA_CONNECTION_SERVICE_SUBHEADER_LINK_INFO		0X05


#define LOLA_LINK_SERVICE_INVALID_SESSION					UINT8_MAX

//TODO: Replace with one time random number.
#define LOLA_LINK_HOST_PMAC									0x0E0F
#define LOLA_LINK_REMOTE_PMAC								0x1A1B
#define LOLA_LINK_SERVICE_INVALID_PMAC						UINT32_MAX

class LoLaLinkService : public IPacketSendService
{
private:
	uint32_t StateStartTime = ILOLA_INVALID_MILLIS;
	uint32_t LastSent = ILOLA_INVALID_MILLIS;

	LoLaServicesManager * ServicesManager = nullptr;

protected:
	class LinkPacketDefinition : public PacketDefinition
	{
	public:
		uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ID; }
		uint8_t GetHeader() { return PACKET_DEFINITION_LINK_HEADER; }
		uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE; }

#ifdef DEBUG_LOLA
		void PrintName(Stream* serial)
		{
			serial->print(F("Link"));
		}
#endif
	} LinkDefinition;

	class LinkPacketAckDefinition : public PacketDefinition
	{
	public:
		uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID; }
		uint8_t GetHeader() { return PACKET_DEFINITION_LINK_ACK_HEADER; }
		uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE; }

#ifdef DEBUG_LOLA
		void PrintName(Stream* serial)
		{
			serial->print(F("LinkAck"));
		}
#endif
	} LinkAckDefinition;

	enum ConnectingEnum : uint8_t
	{
		ConnectingStarting,
		ConnectionGreenLight,
		ConnectionEscalationFailed,
		Size
	};

	uint8_t ConnectingState = 0;

	//Link session variables.
	uint32_t LinkPMAC = LOLA_LINK_SERVICE_INVALID_PMAC;
	uint32_t RemotePMAC = LOLA_LINK_SERVICE_INVALID_PMAC;
	uint8_t SessionId = LOLA_LINK_SERVICE_INVALID_SESSION;

	LoLaLinkInfo LinkInfo;

	//Synced Clock.
	ClockSource * SyncedClock = nullptr;

	//Optimized memory usage grunt packet.
	TemplateLoLaPacket<LOLA_PACKET_SLIM_SIZE> PacketHolder;

	//Subservices.
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
	LoLaDiagnosticsService Diagnostics;
#endif

	//Callback handler.
	Signal<uint8_t> LinkStatusUpdated;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} ATUI;

public:
	LoLaLinkService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_POLL_PERIOD_MILLIS, loLa, &PacketHolder)
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
		, Diagnostics(scheduler, loLa, &LinkInfo)
#endif
	{
		LinkInfo.SetDriver(loLa);
		LinkInfo.LinkState = LoLaLinkInfo::LinkStateEnum::Disabled;
		StateStartTime = Millis();
		RemotePMAC = LOLA_LINK_SERVICE_INVALID_PMAC;
	}

	//Param uint8_t is 
	//LoLaLinkInfo::LinkState enum
	void AttachOnLinkStatusUpdated(const Slot<uint8_t>& slot)
	{
		LinkStatusUpdated.attach(slot);
	}

	bool AddSubServices(LoLaServicesManager * servicesManager)
	{
		if (!IPacketSendService::OnSetup())
		{
			return false;
		}

		ServicesManager = servicesManager;

		if (ServicesManager == nullptr)
		{
			return false;
		}

#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
		if (!ServicesManager->Add(&Diagnostics) || !Diagnostics.AddSubServices(ServicesManager))
		{
			return false;
		}
#endif

		return true;
	}

	bool OnEnable()
	{
		UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Setup);
		return true;
	}

	void OnDisable()
	{
		UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Disabled);
	}

	void OnSendOk(const uint32_t sendDuration)
	{
		LastSent = Millis();
	}

	uint32_t GetElapsedSinceStateStart()
	{
		if (StateStartTime == ILOLA_INVALID_MILLIS)
		{
			return ILOLA_INVALID_MILLIS;
		}
		else
		{
			return Millis() - StateStartTime;
		}
	}

	uint32_t GetElapsedSinceLastSent()
	{
		if (LastSent == ILOLA_INVALID_MILLIS)
		{
			return ILOLA_INVALID_MILLIS;
		}
		else
		{
			return Millis() - LastSent;
		}
	}

	void ResetLastSentTimeStamp()
	{
		LastSent = ILOLA_INVALID_MILLIS;
	}

	LoLaLinkInfo* GetLinkInfo()
	{
		return &LinkInfo;
	}

protected:
	virtual void OnBroadcastReceived(const uint8_t sessionId, uint8_t* data) {}
	virtual void OnChallengeReplyReceived(const uint8_t sessionId, uint8_t* data) {}
	virtual void OnChallengeAcceptedReceived(const uint8_t sessionId, uint8_t* data) {}
	virtual void OnHelloReceived(const uint8_t sessionId, uint8_t* data) {}
	virtual void OnClockReceived(const uint8_t sessionId, uint8_t* data) {}

	virtual void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState) {}
	virtual void OnLinkWarningLow() {}
	virtual void OnLinkWarningMedium() {}
	virtual void OnLinkWarningHigh() {}

	virtual void OnAwaitingConnection() {}
	virtual void OnConnecting() { OnConnectingBasic(); }

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection service"));
	}
#endif // DEBUG_LOLA

	//Link service has the power to always receive packets.
	bool ShouldProcessReceived() { return true; }

	void UpdateLinkState(const LoLaLinkInfo::LinkStateEnum newState)
	{
		if (LinkInfo.LinkState != newState)
		{
			StateStartTime = Millis();
			OnLinkStateChanged(newState);

			//Previous state
			switch (LinkInfo.LinkState)
			{
			case LoLaLinkInfo::LinkStateEnum::Connected:
				//Notify all link dependent services to stop.
				GetLoLa()->SetLinkStatus(false);
				ServicesManager->NotifyServicesLinkUpdated(false);
				LinkInfo.ResetLinkStarted();
				break;
			case LoLaLinkInfo::LinkStateEnum::Setup:
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			case LoLaLinkInfo::LinkStateEnum::Connecting:
			case LoLaLinkInfo::LinkStateEnum::Disabled:
			default:
				break;
			}

			//New state
			switch (newState)
			{
			case LoLaLinkInfo::LinkStateEnum::Setup:
				ClearSession();
				SetNextRunDefault();
				break;
			case LoLaLinkInfo::LinkStateEnum::Connected:
				LinkInfo.StampLinkStarted();
				StateStartTime = Millis();
				ResetLastSentTimeStamp();

				//Notify all link dependent services they can start.
				GetLoLa()->SetLinkStatus(true);
				ServicesManager->NotifyServicesLinkUpdated(true);
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
				ResetLastSentTimeStamp();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
				ResetLastSentTimeStamp();
				break;
			case LoLaLinkInfo::LinkStateEnum::Connecting:
				ResetLastSentTimeStamp();
				break;
			case LoLaLinkInfo::LinkStateEnum::Disabled:
			default:
				break;
			}

			LinkInfo.LinkState = newState;

			LinkStatusUpdated.fire(newState);
		}
	}

	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		if (!packetMap->AddMapping(&LinkDefinition) ||
			!packetMap->AddMapping(&LinkAckDefinition))
		{
			return false;
		}
		
		return true;
	}

	void ClearSession()
	{
		SessionId = LOLA_LINK_SERVICE_INVALID_SESSION;
		RemotePMAC = LOLA_LINK_SERVICE_INVALID_PMAC;
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		if (header == PACKET_DEFINITION_LINK_HEADER)
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
				default:
					break;
				}
			}

			return true;
		}
		else if (header == PACKET_DEFINITION_LINK_ACK_HEADER)
		{
			if (ShouldProcessPackets())
			{
				switch (incomingPacket->GetPayload()[0])
				{
				case LOLA_LINK_SERVICE_ACK_SUBHEADER_HELLO:
					OnHelloReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
					break;
				case LOLA_LINK_SERVICE_ACK_SUBHEADER_NTP:
					OnClockReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
					break;
				default:
					break;
				}
				
			}

			return true;
		}
		return false;
	}

	bool OnSetup()
	{
		if (IPacketSendService::OnSetup())
		{
			SyncedClock = GetLoLa()->GetClockSource();
			if (SyncedClock != nullptr) {
				SetNextRunDefault();
				return true;
			}
		}
		return false;
	}

	void PrepareHello()
	{
		PacketHolder.SetDefinition(&LinkAckDefinition);
		PacketHolder.SetId(SessionId);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SERVICE_ACK_SUBHEADER_HELLO;

		ATUI.uint = LinkPMAC;
		PacketHolder.GetPayload()[1] = ATUI.array[0];
		PacketHolder.GetPayload()[2] = ATUI.array[1];
		PacketHolder.GetPayload()[3] = ATUI.array[2];
		PacketHolder.GetPayload()[4] = ATUI.array[3];
	}

	void PrepareClockSync()
	{
		PacketHolder.SetDefinition(&LinkAckDefinition);
		PacketHolder.SetId(SessionId);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SERVICE_ACK_SUBHEADER_NTP;

		//Actual clock value is set on PreSend().
		PacketHolder.GetPayload()[1] = 0;
		PacketHolder.GetPayload()[2] = 0;
		PacketHolder.GetPayload()[3] = 0;
		PacketHolder.GetPayload()[4] = 0;
	}

	void OnPreSend() 
	{
		ATUI.uint = Millis();
		if (PacketHolder.GetDataHeader == LinkAckDefinition.GetHeader() &&
			PacketHolder.GetPayload()[0] == LOLA_LINK_SERVICE_ACK_SUBHEADER_NTP)
		{
			PacketHolder.GetPayload()[1] = ATUI.array[0];
			PacketHolder.GetPayload()[2] = ATUI.array[1];
			PacketHolder.GetPayload()[3] = ATUI.array[2];
			PacketHolder.GetPayload()[4] = ATUI.array[3];
		}
	}

	virtual bool ShouldProcessPackets()
	{
		if (SessionId != LOLA_LINK_SERVICE_INVALID_SESSION ||
			LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::AwaitingLink ||
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
			if (elapsedSinceLastReceived > LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT)
			{
#ifdef DEBUG_LOLA
				Serial.print(F("Bi directional commms lost over "));
				Serial.print(LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT / 1000);
				Serial.println(F(" seconds."));
#endif
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
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
			SetNextRunASAP();
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			StateStartTime = Millis();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			OnAwaitingConnection();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			SetNextRunASAP();
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
			OnConnecting();
			break;
		case LoLaLinkInfo::LinkStateEnum::Connected:
			if (GetLoLa()->GetLastValidReceivedMillis() != ILOLA_INVALID_MILLIS)
			{
				OnKeepingConnected(Millis() - GetLoLa()->GetLastValidReceivedMillis());
			}
			else
			{
				OnKeepingConnected(0);
			}			
			break;
		case LoLaLinkInfo::LinkStateEnum::Disabled:
		default:
			Disable();
			return;
		}
	}

	void PrepareBasePacket(const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&LinkDefinition);
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
	void OnConnectingBasic()
	{
		if (SessionId == LOLA_LINK_SERVICE_INVALID_SESSION ||
			RemotePMAC == LOLA_LINK_SERVICE_INVALID_PMAC)
		{
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			SetNextRunASAP();
		}
		else
		{
			switch (ConnectingState)
			{
			case ConnectingEnum::ConnectingStarting:
				ConnectingState = ConnectingEnum::ConnectionGreenLight;
				SetNextRunDelay(LOLA_LINK_SERVICE_CONNECTING_MIN_DURATION);
				break;
			case ConnectingEnum::ConnectionGreenLight:
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connected);
				SetNextRunASAP();
				break;
			case ConnectingEnum::ConnectionEscalationFailed:
			default:
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
				SetNextRunASAP();
				break;
			}
		}
	}
};
#endif