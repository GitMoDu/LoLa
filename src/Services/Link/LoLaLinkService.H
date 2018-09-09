// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h

//#define LOLA_LINK_DIAGNOSTICS_ENABLED

#include <Services\Link\LoLaLinkDefinitions.h>

#include <Services\IPacketSendService.h>
#include <Services\LoLaServicesManager.h>
#include <LoLaLinkInfo.h>

#include <Callback.h>
#include <Crypto\TinyCRC.h>
#include <Crypto\LoLaCryptoSeedSource.h>

#include <Services\Link\LoLaLinkClockSyncer.h>
#include <Crypto\PseudoMacGenerator.h>

#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
#include <Services\LoLaDiagnosticsService\LoLaDiagnosticsService.h>
#endif

//TODO: This class is too big, separate layers by funcionality.
// Link Layer
// ClockSyncLayer
class LoLaLinkService : public IPacketSendService
{
private:
	uint32_t StateStartTime = ILOLA_INVALID_MILLIS;
	uint32_t LastSent = ILOLA_INVALID_MILLIS;

	LoLaServicesManager * ServicesManager = nullptr;

protected:
	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
		int32_t iint;
	} ATUI;

	uint32_t ConnectingStateStartTime = 0;

	LoLaCryptoSeedSource CryptoSeed;

	LinkPacketDefinition		LinkDefinition;
	LinkPacketWithAckDefinition	LinkWithAckDefinition;


	uint8_t ConnectingState = 0;

	//MAC Helper
	PseudoMacGenerator PMACGenerator;

	//Link session variables
	uint32_t LinkPMAC = LOLA_LINK_SERVICE_INVALID_PMAC;
	uint32_t RemotePMAC = LOLA_LINK_SERVICE_INVALID_PMAC;
	uint8_t SessionId = LOLA_LINK_SERVICE_INVALID_SESSION;

	LoLaLinkInfo LinkInfo;

	//Synced Clock.
	ILinkClockSyncer* ClockSyncerPointer;

	//Latency measurement.
	uint32_t LastSentWithAck = ILOLA_INVALID_MILLIS;

	//Optimized memory usage grunt packet.
	TemplateLoLaPacket<LOLA_PACKET_SLIM_SIZE> PacketHolder;

	//Subservices.
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
	LoLaDiagnosticsService Diagnostics;
#endif

	//Callback handler.
	Signal<uint8_t> LinkStatusUpdated;

public:
	LoLaLinkService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_POLL_PERIOD_MILLIS, loLa, &PacketHolder)
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
		, Diagnostics(scheduler, loLa, &LinkInfo)
#endif
	{
		PMACGenerator.SetId(loLa->GetIdPointer(), loLa->GetIdLength());
		LinkPMAC = PMACGenerator.GetPMAC();

		LinkInfo.SetDriver(loLa);
		LinkInfo.LinkState = LoLaLinkInfo::LinkStateEnum::Disabled;

		if (loLa != nullptr)
		{
			loLa->SetCryptoSeedSource(&CryptoSeed);
			loLa->SetLinkIndicator(&LinkInfo);
		}

		ClearSession();
	}

	LoLaLinkInfo* GetLinkInfo()
	{
		return &LinkInfo;
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

protected:
	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		//if()//TODO: Catch and measure packet round trip time
		if (SessionId == id && header == PACKET_DEFINITION_LINK_WITH_ACK_HEADER)
		{
			OnLinkPacketAckReceived();
		}
	}

	void OnSendOk(const uint32_t sendDuration)
	{
		LastSent = Millis();
	}

	void ResetStateStartTime()
	{
		StateStartTime = Millis();
	}

	uint32_t GetElapsedSinceStateStart()
	{
		return Millis() - StateStartTime;
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

	void SetConnectingState(const uint8_t connectingState)
	{
		Serial.print("Connecting State: ");
		Serial.println(connectingState);

		ConnectingState = connectingState;
		ResetLastSentTimeStamp();
		ConnectingStateStartTime = Millis();
	}

	void ResetLastSentTimeStamp()
	{
		LastSent = ILOLA_INVALID_MILLIS;
	}

	void TimeStampLastSent()
	{
		LastSent = Millis();
	}

private:
	inline void OnLinkWarningLow()
	{
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD)
		{
			PrepareHello();
			RequestSendPacket();
		}
	}

	inline void OnLinkWarningMedium()
	{
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_FAST_CHECK_PERIOD)
		{
			PrepareHello();
			RequestSendPacket();
		}
	}
	void ClearChallenge()
	{

	}

	void OnHelloReceived(const uint8_t sessionId)
	{
		Serial.println("OnHelloReceived");
		if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::AwaitingSleeping)
		{
			SetNextRunASAP();
		}
	}

protected:
	///Host packet handling.
	virtual void OnLinkRequestReceived(const uint8_t sessionId, const uint32_t remotePMAC) {}
	virtual void OnLinkPacketAckReceived() {}
	virtual void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	///

	///Remote packet handling.
	virtual void OnBroadcastReceived(const uint8_t sessionId, const uint32_t remotePMAC) {}
	virtual void OnLinkRequestAcceptedReceived(const uint8_t sessionId, const uint32_t token) {}
	virtual void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	virtual void OnClockSyncAcceptedReceived(const uint8_t sessionId, const uint32_t token) {}
	///
	
	virtual void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState) {}
	virtual void SetBaseSeed() {}
	virtual void OnLinkWarningHigh() {}
	virtual void OnAwaitingConnection() {}

	//Possibly CPU intensive task.
	virtual bool IsChallengeComplete()
	{
		//TODO: Reset challenge on ClearSession()

		return false;
	}

	virtual void OnClockSync() {}
	virtual void OnClockSyncSwitchOver() {}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection service"));
	}
#endif // DEBUG_LOLA

	bool ShouldProcessReceived()
	{
		return IsSetupOk();
	}

	void UpdateLinkState(const LoLaLinkInfo::LinkStateEnum newState)
	{
		if (LinkInfo.LinkState != newState)
		{
			ResetStateStartTime();
			ResetLastSentTimeStamp();
			CryptoSeed.Reset();

			//Previous state
			switch (LinkInfo.LinkState)
			{
			case LoLaLinkInfo::LinkStateEnum::Connected:
				//Notify all link dependent services to stop.
				ServicesManager->NotifyServicesLinkUpdated(false);
				LinkInfo.ResetLinkStarted();
				break;
			case LoLaLinkInfo::LinkStateEnum::Setup:
				ClearSession();
				SetNextRunDefault();
				break;
			case LoLaLinkInfo::LinkStateEnum::Connecting:
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
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
				CryptoSeed.SetTOTPEnabled(true);
				SetBaseSeed();

				//Notify all link dependent services they can start.
				ServicesManager->NotifyServicesLinkUpdated(true);
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
				SetConnectingState(0);
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
				SetNextRunDelay(LOLA_LINK_SERVICE_SLEEP_PERIOD);
				break;
			case LoLaLinkInfo::LinkStateEnum::Connecting:
				SetBaseSeed();
				SetConnectingState(0);
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::Disabled:
			default:
				break;
			}

			OnLinkStateChanged(newState);
			LinkInfo.LinkState = newState;
			LinkStatusUpdated.fire(newState);
		}
	}

	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		if (!packetMap->AddMapping(&LinkDefinition) ||
			!packetMap->AddMapping(&LinkWithAckDefinition))
		{
			return false;
		}

		return true;
	}

	void ClearSession()
	{
		SessionId = LOLA_LINK_SERVICE_INVALID_SESSION;
		RemotePMAC = LOLA_LINK_SERVICE_INVALID_PMAC;

		ClearChallenge();
		if (ClockSyncerPointer != nullptr)
		{
			ClockSyncerPointer->Reset();
		}	
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		//Switch the packet to the appropriate method.
		switch (header)
		{
		case PACKET_DEFINITION_LINK_HEADER:
			ATUI.array[0] = incomingPacket->GetPayload()[1];
			ATUI.array[1] = incomingPacket->GetPayload()[2];
			ATUI.array[2] = incomingPacket->GetPayload()[3];
			ATUI.array[3] = incomingPacket->GetPayload()[4];

			switch (incomingPacket->GetPayload()[0])
			{
			case LOLA_LINK_SERVICE_SUBHEADER_HELLO:
				OnHelloReceived(incomingPacket->GetId());
				break;
			case LOLA_LINK_SERVICE_SUBHEADER_HOST_BROADCAST:
				OnBroadcastReceived(incomingPacket->GetId(), ATUI.uint);
				break;
			case LOLA_LINK_SERVICE_SUBHEADER_REMOTE_LINK_REQUEST:
				OnLinkRequestReceived(incomingPacket->GetId(), ATUI.uint);
				break;
				//case LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED:
				//	OnChallengeAcceptedReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
				//	break;
			case LOLA_LINK_SERVICE_SUBHEADER_NTP:
				OnClockSyncRequestReceived(incomingPacket->GetId(), ATUI.uint);
				break;
			case LOLA_LINK_SERVICE_SUBHEADER_NTP_REPLY:
				OnClockSyncResponseReceived(incomingPacket->GetId(), ATUI.iint);
				break;
			default:
				break;
			}
			break;
		case PACKET_DEFINITION_LINK_WITH_ACK_HEADER:
			ATUI.array[0] = incomingPacket->GetPayload()[1];
			ATUI.array[1] = incomingPacket->GetPayload()[2];
			ATUI.array[2] = incomingPacket->GetPayload()[3];
			ATUI.array[3] = incomingPacket->GetPayload()[4];

			switch (incomingPacket->GetPayload()[0])
			{
			case LOLA_LINK_SERVICE_SUBHEADER_LINK_REQUEST_ACCEPTED:
				OnLinkRequestAcceptedReceived(incomingPacket->GetId(), ATUI.uint);
				break;
			case LOLA_LINK_SERVICE_SUBHEADER_NTP_ACCEPTED:
				OnClockSyncAcceptedReceived(incomingPacket->GetId(), ATUI.uint);
				break;
			default:
				break;
			}
			break;
		default:
			return false;
		}

		return true;
	}

	bool OnSetup()
	{
		if (IPacketSendService::OnSetup())
		{
			ClearSession();

#ifdef DEBUG_LOLA
			ATUI.uint = LinkPMAC;
			Serial.print(F("Link PMAC: 0x"));
			for (uint8_t i = 0; i < 4; i++)
			{
				Serial.print(ATUI.array[i], HEX);
			}
			Serial.println();
#endif
			StateStartTime = Millis();
			SetNextRunASAP();

			return ClockSyncerPointer != nullptr && ClockSyncerPointer->Setup(GetLoLa()->GetClockSource());

		}
		return false;
	}

	virtual void OnPreSend()
	{
		if (PacketHolder.GetDefinition()->HasACK())
		{
			//Piggy back on any link with ack packets to measure latency.
			LastSentWithAck = GetLoLa()->GetLastSentMillis();
		}
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
			/*	else if (!OnKeepingClockSynced())
				{
					UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
				}*/
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
				SetNextRunDelay(LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD);
				OnLinkWarningLow();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD);
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
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			OnAwaitingConnection();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
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

	inline void ArrayToPayload()
	{
		PacketHolder.GetPayload()[1] = ATUI.array[0];
		PacketHolder.GetPayload()[2] = ATUI.array[1];
		PacketHolder.GetPayload()[3] = ATUI.array[2];
		PacketHolder.GetPayload()[4] = ATUI.array[3];
	}

	void PrepareHello()
	{
		PrepareSessionPacket(LOLA_LINK_SERVICE_SUBHEADER_HELLO);
		ATUI.uint = 0;
		ArrayToPayload();
	}

	void PrepareSessionPacketWithAck(const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&LinkWithAckDefinition);
		PacketHolder.SetId(SessionId);
		PacketHolder.GetPayload()[1] = subHeader;
	}

	void PrepareSessionPacket(const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.SetId(SessionId);
		PacketHolder.GetPayload()[0] = subHeader;
	}

private:
	void OnConnecting()
	{
		if (SessionId == LOLA_LINK_SERVICE_INVALID_SESSION ||
			RemotePMAC == LOLA_LINK_SERVICE_INVALID_PMAC)
		{
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			SetNextRunASAP();
		}
		else if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_BEFORE_CONNECTING_CANCEL)
		{
			//General time out for the connecting state. Prevents any issue with link being stuck
			//in connection limbo.
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			SetNextRunDelay(LOLA_LINK_SERVICE_CONNECTING_CANCEL_SLEEP_DURATION);
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
			switch (ConnectingState)
			{
			case ConnectingStagesEnum::ChallengeStage:
				if (IsChallengeComplete())
				{
					SetConnectingState(ConnectingStagesEnum::ChallengeSwitchOver);
					SetNextRunASAP();
				}
				break;
			case ConnectingStagesEnum::ChallengeSwitchOver:
				///Temporary TODO: Replace with actual challenge ack transition.
				SetConnectingState(ConnectingStagesEnum::ClockSyncStage);
				SetNextRunASAP();

				//TODO:
				//SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
				//We transition forward when we receive the approriate ack.
				break;
			case ConnectingStagesEnum::ClockSyncStage:
				if (ClockSyncerPointer->IsSynced())
				{					
					SetConnectingState(ConnectingStagesEnum::ClockSyncSwitchOver);
					SetNextRunASAP();
				}
				else
				{
					OnClockSync();
				}
				break;
			case ConnectingStagesEnum::ClockSyncSwitchOver:
				OnClockSyncSwitchOver();
				SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
				//We transition forward when we receive the approriate ack.
				break;
			case ConnectingStagesEnum::LinkProtocolStage:
				
				break;
			case ConnectingStagesEnum::ConnectingStagesEnumSize:
				//All connecting stages complete, we have a link.
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connected);
				break;
			default:
				break;
			}
		}
	}

	//void PrepareRssi()
	//{
	//	PrepareShortPacket(LOLA_LINK_SERVICE_SHORT_SUBHEADER_HELLO, SessionId);

	//	//TODO: Add Rssi indicator to tail, for future auto-gain.
	//}

	//TODO:
	//virtual void OnClockSyncWarning()
	//{
	//}

	////Returns false if clock is out of the error margin.
	//inline bool OnKeepingClockSynced()
	//{
	//	if (SyncedClock->GetLastSyncElapsedMillis() > LOLA_LINK_SERVICE_MAX_SYNC_ELAPSED)
	//	{
	//		//Too much time has elapsed since the last sync.
	//		return false;
	//	}
	//	else if (SyncedClock->GetLastSyncElapsedMillis() > LOLA_LINK_SERVICE_MIN_ELAPSED_RESYNC)
	//	{
	//		//Remote should send a NTP refresh, otherwise Host will disconnect.
	//		//TODO: Document in protocol.
	//		OnClockSyncWarning();

	//		return false;
	//	}

	//	return true;
	//}
};
#endif