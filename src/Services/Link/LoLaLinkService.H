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

#include <Services\Link\LoLaLinkCryptoChallenge.h>

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

	bool PingedPending = false;

	LoLaServicesManager * ServicesManager = nullptr;

protected:
	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
		int32_t iint;
	};

	ArrayToUint32 ATUI_R;
	ArrayToUint32 ATUI_S;

	LoLaCryptoSeedSource CryptoSeed;

	LinkPacketDefinition		LinkDefinition;
	LinkPacketWithAckDefinition	LinkWithAckDefinition;

	//Sub state.
	uint8_t ConnectingState = 0;

	//MAC Helper
	PseudoMacGenerator PMACGenerator;

	//Link session variables
	uint32_t RemotePMAC = LOLA_INVALID_PMAC;
	uint8_t SessionId = LOLA_LINK_SERVICE_INVALID_SESSION;

	LoLaLinkInfo LinkInfo;

	//Synced Clock.
	ILinkClockSyncer* ClockSyncerPointer = nullptr;
	IClockSyncTransaction* ClockSyncTransaction = nullptr;

	//Crypto Challenge.
	IChallengeTransaction* ChallengeTransaction = nullptr;

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


protected:
	///Host packet handling.
	virtual void OnLinkDiscoveryReceived() {}
	virtual void OnLinkRequestReceived(const uint8_t sessionId, const uint32_t remotePMAC) {}
	virtual void OnLinkRequestReadyReceived(const uint8_t sessionId, const uint32_t remotePMAC) {}
	virtual void OnLinkPacketAckReceived(const uint8_t requestId) {}
	virtual void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	virtual void OnChallengeResponseReceived(const uint8_t requestId, const uint32_t token) {}
	///

	///Remote packet handling.
	virtual void OnBroadcastReceived(const uint8_t sessionId, const uint32_t remotePMAC) {}
	virtual void OnLinkRequestAcceptedReceived(const uint8_t requestId, const uint32_t localPMAC) {}
	virtual void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	virtual void OnChallengeRequestReceived(const uint8_t requestId, const uint32_t token) {}
	virtual void OnLinkSwitchOverReceived(const uint8_t requestId, const uint8_t subHeader) {}
	///

	virtual void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState) {}
	virtual void SetBaseSeed() {}

	virtual void OnAwaitingConnection() {}

	//Possibly CPU intensive task.
	virtual void OnChallenging() {}
	virtual void OnChallengeSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD); }

	virtual void OnClockSync() {}
	virtual void OnClockSyncSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD); }

	virtual void OnLinkProtocolSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD); }

public:
	LoLaLinkService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_POLL_PERIOD_MILLIS, loLa, &PacketHolder)
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
		, Diagnostics(scheduler, loLa, &LinkInfo)
#endif
	{
		LinkInfo.SetDriver(loLa);
		LinkInfo.LinkState = LoLaLinkInfo::LinkStateEnum::Disabled;

		if (loLa != nullptr)
		{
			loLa->SetCryptoSeedSource(&CryptoSeed);
			loLa->SetLinkIndicator(&LinkInfo);
		}

		CryptoSeed.Reset();

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
		if (header == LinkWithAckDefinition.GetHeader())
		{
			OnLinkPacketAckReceived(id);
		}
	}

	void OnSendOk(const uint8_t header, const uint32_t sendDuration)
	{
		LastSent = Millis();

		if (PingedPending && header == LinkDefinition.GetHeader() &&
			PacketHolder.GetPayload()[0] == LOLA_LINK_SUBHEADER_PONG);
		{
			PingedPending = false;
		}
	}

	void OnSendFailed()
	{
		//In case the send fails, this prevents from immediate resending.
		SetNextRunDelay(LOLA_LINK_SERVICE_LINK_CHECK_PERIOD);
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
		Serial.println(connectingState);

		ConnectingState = connectingState;
		ResetLastSentTimeStamp();
		SetNextRunASAP();
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
	inline void OnLinkWarningHigh()
	{
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_LINK_RESEND_PERIOD)
		{
			PreparePing();
			RequestSendPacket();
		}
	}

	inline void OnLinkWarningLow()
	{
	}

	inline void OnLinkWarningMedium()
	{
		if (GetElapsedSinceLastSent() > LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD)
		{
			PreparePing();
			RequestSendPacket();
		}
	}

	void OnConnecting()
	{
		if (SessionId == LOLA_LINK_SERVICE_INVALID_SESSION ||
			RemotePMAC == LOLA_INVALID_PMAC)
		{
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
			SetNextRunASAP();
		}
		else
		{
			switch (ConnectingState)
			{
			case ConnectingStagesEnum::ClockSyncStage:
				if (ClockSyncerPointer->IsSynced())
				{
					SetConnectingState(ConnectingStagesEnum::ClockSyncSwitchOver);
				}
				else
				{
					OnClockSync();
				}
				break;
			case ConnectingStagesEnum::ClockSyncSwitchOver:
				OnClockSyncSwitchOver();
				//We transition forward when we receive the appropriate message.
				break;
			case ConnectingStagesEnum::ChallengeStage:
				if (ChallengeTransaction->IsChallengeComplete())
				{
					SetConnectingState(ConnectingStagesEnum::ChallengeSwitchOver);
					SetNextRunASAP();
				}
				else
				{
					OnChallenging();
				}
				break;
			case ConnectingStagesEnum::ChallengeSwitchOver:
				OnChallengeSwitchOver();
				break;
			case ConnectingStagesEnum::LinkProtocolSwitchOver:
				OnLinkProtocolSwitchOver();
				break;
			case ConnectingStagesEnum::AllConnectingStagesDone:
				//All connecting stages complete, we have a link.
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Connected);
				break;
			default:
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				SetNextRunASAP();
				break;
			}
		}
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Connection service"));
	}
#endif // DEBUG_LOLA

	bool ShouldProcessReceived()
	{
		if (!IsSetupOk())
		{
			Serial.println("!IsSetupOk()");
		}
		return IsSetupOk();
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
		RemotePMAC = LOLA_INVALID_PMAC;

		if (ClockSyncerPointer != nullptr)
		{
			ClockSyncerPointer->Reset();
		}

		if (ClockSyncTransaction != nullptr)
		{
			ClockSyncTransaction->Reset();
		}

		if (ChallengeTransaction != nullptr)
		{
			ChallengeTransaction->Clear();
		}
	}

#ifdef DEBUG_LOLA
	ArrayToUint32 PmacAtui;
	void PrintPMAC(const uint32_t pmac)
	{
		Serial.print(F("0x"));
		PmacAtui.uint = pmac;
		for (uint8_t i = 0; i < 4; i++)
		{
			Serial.print(PmacAtui.array[i], HEX);
		}
	}
#endif

	bool OnSetup()
	{
		if (IPacketSendService::OnSetup())
		{
			PMACGenerator.SetId(GetLoLa()->GetIdPointer(), GetLoLa()->GetIdLength());
			ClearSession();

#ifdef DEBUG_LOLA
			Serial.print(F("Link PMAC: "));
			PrintPMAC(PMACGenerator.GetPMAC());
			Serial.println();
#endif
			ResetStateStartTime();
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

	void UpdateLinkState(const LoLaLinkInfo::LinkStateEnum newState)
	{
		if (LinkInfo.LinkState != newState)
		{
			ResetStateStartTime();
			ResetLastSentTimeStamp();
			CryptoSeed.Reset();

			//Previous state.
			if (LinkInfo.LinkState == LoLaLinkInfo::LinkStateEnum::Connected)
			{
				//Notify all link dependent services to stop.
				ServicesManager->NotifyServicesLinkUpdated(false);
				LinkInfo.ResetLinkStarted();
			}

			LinkInfo.LinkState = newState;

			//New state.
			switch (LinkInfo.LinkState)
			{
			case LoLaLinkInfo::LinkStateEnum::Setup:
				ClearSession();
				SetNextRunDefault();
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
				Serial.print("Connecting to: 0x");
				Serial.print(RemotePMAC, HEX);
				Serial.print(" Session code: ");
				Serial.println(SessionId);
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::Connected:
				LinkInfo.StampLinkStarted();
				SetBaseSeed();
				CryptoSeed.SetTOTPEnabled(true, ChallengeTransaction->GetToken());
				//Notify all link dependent services they can start.
				ServicesManager->NotifyServicesLinkUpdated(true);
				SetNextRunASAP();

				Serial.print("Link took ");
				Serial.print(millis() - ConnectionProcessStart);
				Serial.println(" ms to establish.");

				Serial.print("Conneted Synced TimeStamp");
				Serial.println(ClockSyncerPointer->GetMillisSync());

				break;
			case LoLaLinkInfo::LinkStateEnum::Disabled:
			default:
				break;
			}

			OnLinkStateChanged(LinkInfo.LinkState);
			LinkStatusUpdated.fire(LinkInfo.LinkState);
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
			if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				SetNextRunASAP();
			}
			else
			{
				OnAwaitingConnection();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			break;
		case LoLaLinkInfo::LinkStateEnum::Connecting:
			if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_BEFORE_CONNECTING_CANCEL)
			{
				Serial.print("Kicked out: ");
				Serial.println(GetElapsedSinceStateStart());
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				SetNextRunASAP();
			}
			else
			{
				OnConnecting();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Connected:
			OnKeepingConnected();
			break;
		case LoLaLinkInfo::LinkStateEnum::Disabled:
		default:
			Disable();
			return;
		}
	}

	uint32_t GetElapsedLastValidReceived()
	{
		if (GetLoLa()->GetLastValidReceivedMillis() != ILOLA_INVALID_MILLIS)
		{
			return Millis() - GetLoLa()->GetLastValidReceivedMillis();
		}
		else
		{
			return 0;
		}
	}	
	
	uint32_t GetElapsedLastSent()
	{
		if (GetLoLa()->GetLastSentMillis() != ILOLA_INVALID_MILLIS)
		{
			return Millis() - GetLoLa()->GetLastSentMillis();
		}
		else
		{
			return 0;
		}
	}

	void OnKeepingConnected()
	{
		if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT)
		{
#ifdef DEBUG_LOLA
			Serial.print(F("Bi directional commms lost over "));
			Serial.print(LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT / 1000);
			Serial.println(F(" seconds."));
#endif
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			SetNextRunASAP();
		}
		else if(PingedPending) {
			PreparePong();
			RequestSendPacket();
			SetNextRunASAP();
		}
		/*	else if (!OnKeepingClockSynced())
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			}*/
		else if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_PANIC)
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
			OnLinkWarningHigh();
		}
		else if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_PERIOD_INTERVENTION)
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
			OnLinkWarningMedium();
		}
		else if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD)
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD);
			OnLinkWarningLow();
		}
		else
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD);
		}
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		//Switch the packet to the appropriate method.
		switch (header)
		{
		case PACKET_DEFINITION_LINK_HEADER:
			//Serial.print("LINK_HEADER: ");
			ATUI_R.array[0] = incomingPacket->GetPayload()[1];
			ATUI_R.array[1] = incomingPacket->GetPayload()[2];
			ATUI_R.array[2] = incomingPacket->GetPayload()[3];
			ATUI_R.array[3] = incomingPacket->GetPayload()[4];

			//Serial.println(ATUI_R.uint);

			switch (incomingPacket->GetPayload()[0])
			{
				//To both.
			case LOLA_LINK_SUBHEADER_LINK_DISCOVERY:
				OnLinkDiscoveryReceived();
				break;
			case LOLA_LINK_SUBHEADER_PING:
				Serial.print("Ping");
				PingedPending = true;
				SetNextRunASAP();
				break;
			case LOLA_LINK_SUBHEADER_PONG:
				Serial.print("Pong");
				SetNextRunASAP();
				break;

				//To Host.
			case LOLA_LINK_SUBHEADER_REMOTE_LINK_REQUEST:
				OnLinkRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;
			case LOLA_LINK_SUBHEADER_REMOTE_LINK_READY:
				OnLinkRequestReadyReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;
			case LOLA_LINK_SUBHEADER_CHALLENGE_REPLY:
				OnChallengeResponseReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;
			case LOLA_LINK_SUBHEADER_NTP_REQUEST:
				OnClockSyncRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;

				//To remote.
			case LOLA_LINK_SUBHEADER_HOST_BROADCAST:
				OnBroadcastReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;
			case LOLA_LINK_SUBHEADER_HOST_LINK_ACCEPTED:
				OnLinkRequestAcceptedReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;
			case LOLA_LINK_SUBHEADER_NTP_REPLY:
				OnClockSyncResponseReceived(incomingPacket->GetId(), ATUI_R.iint);
				break;
			case LOLA_LINK_SUBHEADER_CHALLENGE_REQUEST:
				OnChallengeRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;
			default:
				break;
			}
			break;
		case PACKET_DEFINITION_LINK_WITH_ACK_HEADER:
			//To remote.
			OnLinkSwitchOverReceived(incomingPacket->GetId(), incomingPacket->GetPayload()[0]);
			break;
		default:
			return false;
		}

		return true;
	}

	///Packet builders.
	inline void ArrayToPayload()
	{
		PacketHolder.GetPayload()[1] = ATUI_S.array[0];
		PacketHolder.GetPayload()[2] = ATUI_S.array[1];
		PacketHolder.GetPayload()[3] = ATUI_S.array[2];
		PacketHolder.GetPayload()[4] = ATUI_S.array[3];
	}

	void PrepareLinkDiscovery()
	{
		PrepareLinkPacket(SessionId, LOLA_LINK_SUBHEADER_LINK_DISCOVERY);
		ATUI_S.uint = 0;
		ArrayToPayload();
	}

	void PreparePing()
	{
		PrepareLinkPacket(0, LOLA_LINK_SUBHEADER_PING);
		ATUI_S.uint = 0;
		ArrayToPayload();
	}

	void PreparePong()
	{
		PrepareLinkPacket(0, LOLA_LINK_SUBHEADER_PONG);
		ATUI_S.uint = 0;
		ArrayToPayload();
	}

	void PreparePacketBroadcast()							//Host.
	{
		PrepareLinkPacket(SessionId, LOLA_LINK_SUBHEADER_HOST_BROADCAST);
		ATUI_S.uint = PMACGenerator.GetPMAC();
		ArrayToPayload();
	}

	void PrepareLinkRequest()								//Remote.
	{
		PrepareLinkPacket(SessionId, LOLA_LINK_SUBHEADER_REMOTE_LINK_REQUEST);
		ATUI_S.uint = PMACGenerator.GetPMAC();
		ArrayToPayload();
	}

	void PrepareLinkRequestAccepted()						//Host.
	{
		PrepareLinkPacket(SessionId, LOLA_LINK_SUBHEADER_HOST_LINK_ACCEPTED);
		ATUI_S.uint = RemotePMAC;
		ArrayToPayload();
	}

	void PrepareLinkRequestReady()							//Remote.
	{
		PrepareLinkPacket(SessionId, LOLA_LINK_SUBHEADER_REMOTE_LINK_READY);
		ATUI_S.uint = PMACGenerator.GetPMAC();
		ArrayToPayload();
	}

	void PrepareLinkConnectingSwitchOver()					//Host.
	{
		PrepareLinkPacketWithAck(SessionId, LOLA_LINK_SUBHEADER_ACK_LINK_REQUEST_SWITCHOVER);
	}

	void PrepareClockSyncRequest(const uint8_t requestId)	//Remote
	{
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SUBHEADER_NTP_REQUEST;

		//Rest of Payload is set on OnPreSend.
	}

	void PrepareClockSyncResponse(const uint8_t requestId, const uint32_t estimationError)
	{														//Host
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SUBHEADER_NTP_REPLY;

		ATUI_S.uint = estimationError;
		ArrayToPayload();
	}

	void PrepareClockSyncSwitchOver()						//Host.
	{
		PrepareLinkPacketWithAck(LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER, LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER);
	}

	void PrepareChallengeRequest()							//Host.
	{
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SUBHEADER_CHALLENGE_REQUEST;
		PacketHolder.SetId(ChallengeTransaction->GetTransactionId());
		ATUI_S.uint = ChallengeTransaction->GetToken();
		ArrayToPayload();
	}

	void PrepareChallengeReply()							//Remote.
	{
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SUBHEADER_CHALLENGE_REPLY;
		PacketHolder.SetId(ChallengeTransaction->GetTransactionId());
		ATUI_S.uint = ChallengeTransaction->GetToken();
		ArrayToPayload();
	}

	void PrepareChallengeSwitchOver()						//Host.
	{
		PrepareLinkPacketWithAck(ChallengeTransaction->GetTransactionId(), LOLA_LINK_SUBHEADER_ACK_CHALLENGE_SWITCHOVER);
	}

	void PrepareLinkProtocolSwitchOver()					//Host.
	{
		PrepareLinkPacketWithAck(SessionId, LOLA_LINK_SUBHEADER_ACK_PROTOCOL_SWITCHOVER);
	}

private:
	inline void PrepareLinkPacketWithAck(const uint8_t requestId, const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&LinkWithAckDefinition);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = subHeader;
	}

	inline void PrepareLinkPacket(const uint8_t requestId, const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = subHeader;
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
	///
};
#endif