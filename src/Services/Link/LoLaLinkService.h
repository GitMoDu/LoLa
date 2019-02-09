// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h

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
#include <Services\Link\LoLaLinkInfoSyncTransaction.h>

#include <Services\Link\LoLaLinkPowerBalancer.h>

#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
#include <Services\LoLaDiagnosticsService\LoLaDiagnosticsService.h>
#endif

class LoLaLinkService : public IPacketSendService
{
private:
	uint32_t StateStartTime = ILOLA_INVALID_MILLIS;
	uint32_t LastSent = ILOLA_INVALID_MILLIS;

	bool PingedPending = false;

	LoLaServicesManager * ServicesManager = nullptr;

	LoLaLinkPowerBalancer PowerBalancer;

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
	LoLaLinkClockSyncer* ClockSyncerPointer = nullptr;
	IClockSyncTransaction* ClockSyncTransaction = nullptr;

	//Crypto Challenge.
	IChallengeTransaction* ChallengeTransaction = nullptr;

	//Link Info Sync
	InfoSyncTransaction* InfoTransaction = nullptr;

	//Optimized memory usage grunt packet.
	TemplateLoLaPacket<LOLA_PACKET_SLIM_SIZE> PacketHolder;

#ifdef DEBUG_LOLA
	uint32_t ConnectionProcessStart = 0;
#endif

	//Subservices.
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
	LoLaDiagnosticsService Diagnostics;
#endif

protected:
	///Common packet handling.
	virtual void OnLinkInfoSyncUpdateReceived(const uint8_t contentId, const uint32_t content) {}
	///Host packet handling.
	virtual void OnLinkDiscoveryReceived() {}
	virtual void OnLinkRequestReceived(const uint8_t sessionId, const uint32_t remotePMAC) {}
	virtual void OnLinkRequestReadyReceived(const uint8_t sessionId, const uint32_t remotePMAC) {}
	virtual void OnLinkPacketAckReceived(const uint8_t requestId) {}
	virtual void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	virtual void OnChallengeResponseReceived(const uint8_t requestId, const uint32_t token) {}
	virtual void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	///

	///Remote packet handling.
	virtual void OnBroadcastReceived(const uint8_t sessionId, const uint32_t remotePMAC) {}
	virtual void OnLinkRequestAcceptedReceived(const uint8_t requestId, const uint32_t localPMAC) {}
	virtual void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	virtual void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	virtual void OnChallengeRequestReceived(const uint8_t requestId, const uint32_t token) {}
	///

	virtual void SetBaseSeed() {}
	virtual void OnClearSession() {};
	virtual void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState) {}
	virtual void OnAwaitingConnection() {}
	virtual void OnChallenging() {}
	virtual void OnChallengeSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD); }
	virtual void OnClockSync() {}
	virtual void OnInfoSync() {}
	virtual void OnClockSyncSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD); }
	virtual void OnLinkProtocolSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD); }
	virtual void OnLinkingSwitchOverReceived(const uint8_t requestId, const uint8_t subHeader) {}
	virtual void OnKeepingConnected() { SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD); }

private:
	inline void TrySendPing(const uint32_t resendPeriod)
	{
		if (GetElapsedSinceLastSent() > resendPeriod)
		{
			PreparePing();
			RequestSendPacket();
		}
	}

	void OnConnecting()
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
			if (ChallengeTransaction->IsComplete())
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
		case ConnectingStagesEnum::InfoSyncStage:
			if (InfoTransaction->IsComplete())
			{
				SetConnectingState(ConnectingStagesEnum::LinkProtocolSwitchOver);
				SetNextRunASAP();
			}
			else
			{
				OnInfoSync();
			}	
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

public:
	LoLaLinkService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_FAST_CHECK_PERIOD, loLa, &PacketHolder)
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
		, Diagnostics(scheduler, loLa, &LinkInfo)
#endif
	{
		LinkInfo.SetDriver(loLa);
		LinkInfo.Reset();

		if (loLa != nullptr)
		{
			loLa->SetCryptoSeedSource(&CryptoSeed);
			loLa->SetLinkIndicator(&LinkInfo);
		}

	}

	LoLaLinkInfo* GetLinkInfo()
	{
		return &LinkInfo;
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
	void OnSendOk(const uint8_t header, const uint32_t sendDuration)
	{
		LastSent = Millis();

		if (PingedPending && header == LinkDefinition.GetHeader() &&
			PacketHolder.GetPayload()[0] == LOLA_LINK_SUBHEADER_PONG)
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

	inline void SetTOTPEnabled()
	{
		SetBaseSeed();
		CryptoSeed.SetTOTPEnabled(true, GetLoLa()->GetClockSource(), ChallengeTransaction->GetToken());
	}

	inline void SetConnectingState(const uint8_t connectingState)
	{
		ConnectingState = connectingState;
		ResetLastSentTimeStamp();
		if (ConnectingState == ConnectingStagesEnum::LinkProtocolSwitchOver)
		{
			//The last step of establishing a Link is to exchange a packet with TOTP enabled.
			SetTOTPEnabled();
		}
			
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

		if (InfoTransaction != nullptr)
		{
			InfoTransaction->Clear();
		}

		OnClearSession();
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
		if (IPacketSendService::OnSetup() &&
			ClockSyncerPointer != nullptr &&
			ClockSyncerPointer->Setup(GetLoLa()->GetClockSource()) &&
			PMACGenerator.SetId(GetLoLa()->GetIdPointer(), GetLoLa()->GetIdLength()) &&
			PowerBalancer.Setup(GetLoLa()))
		{
			ClearSession();

#ifdef DEBUG_LOLA
			Serial.print(F("Link PMAC: "));
			PrintPMAC(PMACGenerator.GetPMAC());
			Serial.println();
#endif

			CryptoSeed.Reset();
			ResetStateStartTime();
			SetNextRunASAP();

			return true;

		}

		return false;
	}

	void UpdateLinkState(const LoLaLinkInfo::LinkStateEnum newState)
	{
		if (LinkInfo.GetLinkState() != newState)
		{
			ResetStateStartTime();
			ResetLastSentTimeStamp();

			//Previous state.
			if (LinkInfo.GetLinkState() == LoLaLinkInfo::LinkStateEnum::Connected)
			{
#ifdef DEBUG_LOLA
				Serial.print("Lost connection after ");
				Serial.print(LinkInfo.GetLinkDuration() / (uint32_t)1000);
				Serial.print(F(" seconds."));
#endif
				//Notify all link dependent services to stop.
				ServicesManager->NotifyServicesLinkUpdated(false);
				LinkInfo.ResetLinkStarted();
				PowerBalancer.Reset();
			}

			OnLinkStateChanged(newState);

			switch (newState)
			{
			case LoLaLinkInfo::LinkStateEnum::Setup:
				ClearSession();
				CryptoSeed.Reset();
				SetNextRunDefault();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
				CryptoSeed.Reset();
				SetConnectingState(0);
				PowerBalancer.Reset();
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
				CryptoSeed.Reset();
				SetNextRunDelay(LOLA_LINK_SERVICE_SLEEP_PERIOD);
				break;
			case LoLaLinkInfo::LinkStateEnum::Connecting:
				SetBaseSeed();
				SetConnectingState(0);
#ifdef DEBUG_LOLA
				Serial.print("Connecting to: 0x");
				PrintPMAC(RemotePMAC);
				Serial.println();
				Serial.print(" Session code: ");
				Serial.println(SessionId);
				Serial.print("Connecting Seed: ");
				Serial.println(CryptoSeed.GetToken());
				Serial.print("Last RSSI: ");
				Serial.println(GetLoLa()->GetLastValidRSSI());
#endif
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::Connected:
				LinkInfo.StampLinkStarted();
				SetBaseSeed();
				CryptoSeed.SetTOTPEnabled(true, GetLoLa()->GetClockSource(), ChallengeTransaction->GetToken());
				SetNextRunASAP();
				ClockSyncerPointer->StampSynced();

#ifdef DEBUG_LOLA
				Serial.print(F("Link took "));
				Serial.print(millis() - ConnectionProcessStart);
				Serial.println(F(" ms to establish."));
				Serial.print(F("Average latency: "));
				Serial.print((float)LinkInfo.GetRTT() / (float)2000, 2);
				Serial.println(F(" ms"));
				Serial.print(F("Remote RSSI: "));
				Serial.print(LinkInfo.GetRemoteRSSINormalized());
				Serial.println();
				Serial.print(F("Sync TimeStamp: "));
				Serial.println(ClockSyncerPointer->GetMillisSync());
#endif

				//Notify all link dependent services they can start.
				ServicesManager->NotifyServicesLinkUpdated(true);
				break;
			case LoLaLinkInfo::LinkStateEnum::Disabled:
				LinkInfo.Reset();
			default:
				break;
			}

			LinkInfo.UpdateState(newState);
		}
	}

	void OnService()
	{
		switch (LinkInfo.GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::Setup:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP)
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
			if (SessionId == LOLA_LINK_SERVICE_INVALID_SESSION ||
				RemotePMAC == LOLA_INVALID_PMAC ||
				GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_BEFORE_CONNECTING_CANCEL)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				SetNextRunASAP();
			}
			else
			{
				OnConnecting();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Connected:
			OnKeepingConnectedInternal();
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

	inline void OnKeepingConnectedInternal()
	{
		if (PingedPending)
		{
			PreparePong();
			RequestSendPacket();
			SetNextRunASAP();
		}
		else if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT)
		{
			//Link time out check.
#ifdef DEBUG_LOLA
			Serial.print(F("Bi directional commms lost over "));
			Serial.print(LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT / 1000);
			Serial.println(F(" seconds."));
#endif
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			SetNextRunASAP();
		}
		else if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_PANIC)
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
			TrySendPing(LOLA_LINK_SERVICE_LINK_RESEND_PERIOD);
		}
		else if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_PERIOD_INTERVENTION)
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_FAST_CHECK_PERIOD);
			TrySendPing(LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD);
		}
		//else if ()
		//{
		//TODO: Handle power balancer.
		//}
		else
		{
			OnKeepingConnected();
		}
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		//Switch the packet to the appropriate method.
		switch (header)
		{
		case PACKET_DEFINITION_LINK_HEADER:
			ATUI_R.array[0] = incomingPacket->GetPayload()[1];
			ATUI_R.array[1] = incomingPacket->GetPayload()[2];
			ATUI_R.array[2] = incomingPacket->GetPayload()[3];
			ATUI_R.array[3] = incomingPacket->GetPayload()[4];

			switch (incomingPacket->GetPayload()[0])
			{
				//To both.
			case LOLA_LINK_SUBHEADER_PING:
				PingedPending = true;
				SetNextRunASAP();
				break;
			case LOLA_LINK_SUBHEADER_PONG:
				SetNextRunASAP();
				break;
			case LOLA_LINK_SUBHEADER_INFO_SYNC_UPDATE:
				OnLinkInfoSyncUpdateReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;

				//To Host.
			case LOLA_LINK_SUBHEADER_LINK_DISCOVERY:
				OnLinkDiscoveryReceived();
				break;
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
			case LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST:
				OnClockSyncTuneRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
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
			case LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY:
				OnClockSyncTuneResponseReceived(incomingPacket->GetId(), ATUI_R.iint);
				break;
			case LOLA_LINK_SUBHEADER_CHALLENGE_REQUEST:
				OnChallengeRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;
			default:
				break;
			}
			break;
		case PACKET_DEFINITION_LINK_WITH_ACK_HEADER:
			//To both.
			OnLinkingSwitchOverReceived(incomingPacket->GetId(), incomingPacket->GetPayload()[0]);
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

	void PrepareClockSyncTuneRequest(const uint8_t requestId)	//Remote
	{
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST;

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

	void PrepareClockSyncTuneResponse(const uint8_t requestId, const uint32_t estimationError)
	{														//Host
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY;

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

	void PrepareLinkInfoSyncUpdate(const uint8_t contentId, const uint32_t content) //Host
	{
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.SetId(contentId);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SUBHEADER_INFO_SYNC_UPDATE;

		ATUI_S.uint = content;
		ArrayToPayload();
	}
	
	void PrepareLinkInfoSyncAdvanceRequest()						//Both.
	{
		PrepareLinkPacketWithAck(LOLA_LINK_SUBHEADER_ACK_INFO_SYNC_SWITCHOVER, LOLA_LINK_SUBHEADER_ACK_INFO_SYNC_SWITCHOVER);
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
};
#endif