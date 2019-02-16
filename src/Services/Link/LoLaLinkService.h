// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h

#include <Services\Link\LoLaLinkDefinitions.h>

#include <Services\IPacketSendService.h>
#include <Services\LoLaServicesManager.h>
#include <LoLaLinkInfo.h>

#include <Callback.h>
#include <Crypto\TinyCRC.h>
#include <Crypto\LoLaCryptoTokenSource.h>

#include <Services\Link\LoLaLinkClockSyncer.h>
#include <Crypto\PseudoMacGenerator.h>

#include <Services\Link\LoLaLinkCryptoChallenge.h>
#include <Services\Link\LoLaLinkInfoSyncTransaction.h>

#include <Services\Link\LoLaLinkPowerBalancer.h>

#ifdef USE_FREQUENCY_HOP
#include <Services\Link\LoLaLinkFrequencyHopper.h>
#endif

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

	//Sub-services.
	LoLaLinkPowerBalancer PowerBalancer;

#ifdef USE_FREQUENCY_HOP
	LoLaLinkFrequencyHopper FrequencyHopper;
#endif
	
#ifdef DEBUG_LOLA
	uint32_t LastDebugged = ILOLA_INVALID_MILLIS;
#endif

protected:
	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
		int32_t iint;
	};

	ArrayToUint32 ATUI_R;
	ArrayToUint32 ATUI_S;

	LoLaCryptoTokenSource CryptoSeed;

	LinkPacketDefinition		LinkDefinition;
	LinkPacketWithAckDefinition	LinkWithAckDefinition;

	//Sub state.
	uint8_t LinkingState = 0;

	///MAC generated from UUID.
	LoLaMAC<LOLA_LINK_INFO_MAC_LENGTH> MacManager;
	///

	LoLaLinkInfo* LinkInfo = nullptr;

	//Synced Clock.
	LoLaLinkClockSyncer* ClockSyncerPointer = nullptr;
	IClockSyncTransaction* ClockSyncTransaction = nullptr;

	//Crypto Challenge.
	IChallengeTransaction* ChallengeTransaction = nullptr;

	//Link Info Sync.
	InfoSyncTransaction* InfoTransaction = nullptr;

	//Optimized memory usage grunt packet.
	TemplateLoLaPacket<(LOLA_PACKET_MIN_SIZE + 5)> PacketHolder;

#ifdef DEBUG_LOLA
	uint32_t LinkingStart = 0;
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
	virtual void OnLinkRequestReceived(const uint8_t sessionId, uint8_t* remoteMAC) {}
	virtual void OnLinkRequestReadyReceived(const uint8_t sessionId, uint8_t* remoteMAC) {}
	virtual void OnLinkPacketAckReceived(const uint8_t requestId) {}
	virtual void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	virtual void OnChallengeResponseReceived(const uint8_t requestId, const uint32_t token) {}
	virtual void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	///

	///Remote packet handling.
	virtual void OnBroadcastReceived(const uint8_t sessionId, uint8_t* hostMAC) {}
	virtual void OnLinkRequestAcceptedReceived(const uint8_t requestId, uint8_t* localMAC) {}
	virtual void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	virtual void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	virtual void OnChallengeRequestReceived(const uint8_t requestId, const uint32_t token) {}
	///

	virtual void SetBaseSeed() {}
	virtual void OnClearSession() {};
	virtual void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState) {}
	virtual void OnAwaitingConnection() {}
	virtual void OnChallenging() {}
	virtual void OnChallengeSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }
	virtual void OnClockSync() {}
	virtual void OnInfoSync() {}
	virtual void OnClockSyncSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }
	virtual void OnLinkProtocolSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }
	virtual void OnLinkingSwitchOverReceived(const uint8_t requestId, const uint8_t subHeader) {}
	virtual void OnKeepingConnected() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }

private:
	inline void TrySendPing(const uint32_t resendPeriod)
	{
		if (GetElapsedSinceLastSent() > resendPeriod)
		{
			PreparePing();
			RequestSendPacket();
		}
	}

	void OnLinking()
	{
		switch (LinkingState)
		{
		case LinkingStagesEnum::ClockSyncStage:
			if (ClockSyncerPointer->IsSynced())
			{
				SetLinkingState(LinkingStagesEnum::ClockSyncSwitchOver);
			}
			else
			{
				OnClockSync();
			}
			break;
		case LinkingStagesEnum::ClockSyncSwitchOver:
			OnClockSyncSwitchOver();
			//We transition forward when we receive the appropriate message.
			break;
		case LinkingStagesEnum::ChallengeStage:
			if (ChallengeTransaction->IsComplete())
			{
				SetLinkingState(LinkingStagesEnum::ChallengeSwitchOver);
				SetNextRunASAP();
			}
			else
			{
				OnChallenging();
			}
			break;
		case LinkingStagesEnum::ChallengeSwitchOver:
			OnChallengeSwitchOver();
			break;
		case LinkingStagesEnum::InfoSyncStage:
			if (InfoTransaction->IsComplete())
			{
				SetLinkingState(LinkingStagesEnum::LinkProtocolSwitchOver);
				SetNextRunASAP();
			}
			else
			{
				OnInfoSync();
			}
			break;
		case LinkingStagesEnum::LinkProtocolSwitchOver:
			OnLinkProtocolSwitchOver();
			break;
		case LinkingStagesEnum::AllConnectingStagesDone:
			//All connecting stages complete, we have a link.
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linked);
			break;
		default:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
			SetNextRunASAP();
			break;
		}
	}

public:
	LoLaLinkService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_CHECK_PERIOD, loLa, &PacketHolder)
#ifdef USE_FREQUENCY_HOP
		, FrequencyHopper(scheduler, loLa)
#endif
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
		, Diagnostics(scheduler, loLa, &LinkInfo)
#endif
	{
	}

	bool SetServicesManager(LoLaServicesManager * servicesManager)
	{
		ServicesManager = servicesManager;

#ifdef USE_FREQUENCY_HOP
		if (ServicesManager != nullptr)
		{
			return ServicesManager->Add(&FrequencyHopper);
		}
		return false;
#else
		return true;
#endif
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
		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	}

	void ResetStateStartTime()
	{
		StateStartTime = Millis();
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

	void SetTOTPEnabled()
	{
		SetBaseSeed();
		CryptoSeed.SetTOTPEnabled(true, GetLoLa()->GetClockSource(), ChallengeTransaction->GetToken());
	}

	void SetLinkingState(const uint8_t linkingState)
	{
		LinkingState = linkingState;
		ResetLastSentTimeStamp();
		if (LinkingState == LinkingStagesEnum::LinkProtocolSwitchOver)
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
		serial->print(F("Link service"));
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

		if (LinkInfo != nullptr)
		{
			LinkInfo->Reset();
		}

		OnClearSession();

#ifdef DEBUG_LOLA
		LastDebugged = ILOLA_INVALID_MILLIS;
#endif
	}

	bool OnSetup()
	{
		if (IPacketSendService::OnSetup() &&
			ClockSyncTransaction != nullptr &&
			ClockSyncerPointer != nullptr &&
			ChallengeTransaction != nullptr &&
			ServicesManager != nullptr &&
			MacManager.GetMACPointer() != nullptr &&
			ClockSyncerPointer->Setup(GetLoLa()->GetClockSource()) &&
			PowerBalancer.Setup(GetLoLa()))
		{
			LinkInfo = ServicesManager->GetLinkInfo();

			if (LinkInfo != nullptr)
			{
				LinkInfo->SetDriver(GetLoLa());
				LinkInfo->Reset();

				LinkInfo->SetLocalMAC(MacManager.GetMACPointer());
				GetLoLa()->SetCryptoSeedSource(&CryptoSeed);
#ifdef USE_FREQUENCY_HOP
				FrequencyHopper.SetCryptoSeedSource(&CryptoSeed);
#endif

				ClearSession();

#ifdef DEBUG_LOLA
				Serial.print(F("Local MAC: "));
				MacManager.Print(&Serial);
				Serial.println();
#endif

				CryptoSeed.Reset();
				ResetStateStartTime();
				SetNextRunASAP();

				return true;
			}

		}

		return false;
	}

#ifdef DEBUG_LOLA
	void DebugLinkStatistics(Stream* serial)
	{
		serial->println();
		serial->println(F("Link Info"));

		serial->print(F("UpTime: "));

		uint32_t AliveSeconds = (LinkInfo->GetLinkDuration() / 1000L);

		if (AliveSeconds / 3600 < 10)
			serial->print('0');
		serial->print(AliveSeconds / 3600);
		serial->print(':');
		if ((AliveSeconds % 3600) / 60 < 10)
			serial->print('0');
		serial->print((AliveSeconds % 3600) / 60);
		serial->print(':');
		if ((AliveSeconds % 3600) % 60 < 10)
			serial->print('0');
		serial->println((AliveSeconds % 3600) % 60);

		serial->print(F("Sent: "));
		serial->println(GetLoLa()->GetSentCount());
		serial->print(F("Received: "));
		serial->println(GetLoLa()->GetReceivedCount());
		serial->print(F("Rejected: "));
		serial->print(GetLoLa()->GetRejectedCount());
		serial->print(F(" ("));
		if (GetLoLa()->GetReceivedCount() > 0)
		{
			serial->print((float)(GetLoLa()->GetRejectedCount() * 100) / (float)GetLoLa()->GetReceivedCount(), 2);
			serial->println(F(" %)"));
		}
		else if(GetLoLa()->GetRejectedCount()> 0)
		{
			serial->println(F("100 %)"));
		}
		else 
		{
			serial->println(F("0 %)"));
		}

		serial->print(F("ClockSync adjustments: "));
		serial->println(LinkInfo->GetClockSyncAdjustments());
		serial->println();
#endif
	}

	void UpdateLinkState(const LoLaLinkInfo::LinkStateEnum newState)
	{
		if (LinkInfo->GetLinkState() != newState)
		{
			ResetStateStartTime();
			ResetLastSentTimeStamp();

			//Previous state.
			if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linked)
			{
#ifdef DEBUG_LOLA
				DebugLinkStatistics(&Serial);
#endif
				//Notify all link dependent services to stop.
				ServicesManager->NotifyServicesLinkUpdated(false);
				LinkInfo->Reset();
				PowerBalancer.Reset();
			}

			OnLinkStateChanged(newState);

			switch (newState)
			{
			case LoLaLinkInfo::LinkStateEnum::Setup:
				ClearSession();
				CryptoSeed.Reset();
				SetNextRunDefault();
#ifdef USE_FREQUENCY_HOP
				FrequencyHopper.ResetChannel();
#endif
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
				CryptoSeed.Reset();
				SetLinkingState(0);
				PowerBalancer.Reset();
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
				CryptoSeed.Reset();
				SetNextRunDelay(LOLA_LINK_SERVICE_SLEEP_PERIOD);
				break;
			case LoLaLinkInfo::LinkStateEnum::Linking:
				SetBaseSeed();
				SetLinkingState(0);
#ifdef DEBUG_LOLA
				LinkingStart = millis();
				Serial.print(F("Linking Session "));
				Serial.println(LinkInfo->GetSessionId());
				Serial.print(F("To: "));
				MacManager.Print(&Serial, LinkInfo->GetPartnerMAC());
				Serial.println();
#endif
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::Linked:
				LinkInfo->StampLinkStarted();
				GetLoLa()->ResetStatistics();
				SetTOTPEnabled();
				SetNextRunASAP();
				ClockSyncerPointer->StampSynced();

#ifdef DEBUG_LOLA
				Serial.print(F("Linking took "));
				Serial.print(millis() - LinkingStart);
				Serial.println(F(" ms."));
				Serial.print(F("Round Trip Time: "));
				Serial.print(LinkInfo->GetRTT());
				Serial.println(F(" us"));
				Serial.print(F("Estimated Latency: "));
				Serial.print((float)LinkInfo->GetRTT() / (float)2000, 2);
				Serial.println(F(" ms"));
#ifdef USE_LATENCY_COMPENSATION
				Serial.print(F("Latency compensation: "));
				Serial.print(GetLoLa()->GetETTM());
				Serial.println(F(" ms"));
#endif
				Serial.print(F("Sync TimeStamp: "));
				Serial.println(ClockSyncerPointer->GetMillisSync());
				Serial.println();
#endif

				//Notify all link dependent services they can start.
				ServicesManager->NotifyServicesLinkUpdated(true);
				break;
			case LoLaLinkInfo::LinkStateEnum::Disabled:
				LinkInfo->Reset();
			default:
				break;
			}

			LinkInfo->UpdateState(newState);
		}
	}

	void OnService()
	{
		switch (LinkInfo->GetLinkState())
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
		case LoLaLinkInfo::LinkStateEnum::Linking:
			if (!LinkInfo->HasSessionId() ||
				!LinkInfo->HasPartnerMAC() ||
				GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_MAX_BEFORE_CONNECTING_CANCEL)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				SetNextRunASAP();
			}
			else
			{
				OnLinking();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
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
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			SetNextRunASAP();
		}
		else if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_PANIC)
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			TrySendPing(LOLA_LINK_SERVICE_PANIC);
		}
		else if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_PERIOD_INTERVENTION)
		{
			SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			TrySendPing(LOLA_LINK_SERVICE_PERIOD_INTERVENTION);
		}
		//else if ()
		//{
		//TODO: Handle power balancer.
		//}
		else
		{
			OnKeepingConnected();
		}

#ifdef DEBUG_LOLA
		if (LinkInfo->HasLink())
		{
			if (LastDebugged == ILOLA_INVALID_MILLIS)
			{
				LastDebugged = MillisSync();
			}
			else if (MillisSync() - LastDebugged > (LOLA_LINK_DEBUG_UPDATE_SECONDS * 1000))
			{
				DebugLinkStatistics(&Serial);
				LastDebugged = MillisSync();
			}
		}
#endif
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		//Switch the packet to the appropriate method.
		switch (header)
		{
		case PACKET_DEFINITION_LINK_HEADER:
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
				ArrayTo32BitArray(&incomingPacket->GetPayload()[1]);
				OnLinkInfoSyncUpdateReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;

				//To Host.
			case LOLA_LINK_SUBHEADER_LINK_DISCOVERY:
				OnLinkDiscoveryReceived();
				break;
			case LOLA_LINK_SUBHEADER_REMOTE_LINK_REQUEST:
				OnLinkRequestReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
				break;
			case LOLA_LINK_SUBHEADER_REMOTE_LINK_READY:
				OnLinkRequestReadyReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
				break;
			case LOLA_LINK_SUBHEADER_CHALLENGE_REPLY:
				ArrayTo32BitArray(&incomingPacket->GetPayload()[1]);
				OnChallengeResponseReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;
			case LOLA_LINK_SUBHEADER_NTP_REQUEST:
				ArrayTo32BitArray(&incomingPacket->GetPayload()[1]);
				OnClockSyncRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;
			case LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST:
				ArrayTo32BitArray(&incomingPacket->GetPayload()[1]);
				OnClockSyncTuneRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
				break;

				//To remote.
			case LOLA_LINK_SUBHEADER_HOST_BROADCAST:
				OnBroadcastReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
				break;
			case LOLA_LINK_SUBHEADER_HOST_LINK_ACCEPTED:
				OnLinkRequestAcceptedReceived(incomingPacket->GetId(), &incomingPacket->GetPayload()[1]);
				break;
			case LOLA_LINK_SUBHEADER_NTP_REPLY:
				ArrayTo32BitArray(&incomingPacket->GetPayload()[1]);
				OnClockSyncResponseReceived(incomingPacket->GetId(), ATUI_R.iint);
				break;
			case LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY:
				ArrayTo32BitArray(&incomingPacket->GetPayload()[1]);
				OnClockSyncTuneResponseReceived(incomingPacket->GetId(), ATUI_R.iint);
				break;
			case LOLA_LINK_SUBHEADER_CHALLENGE_REQUEST:
				ArrayTo32BitArray(&incomingPacket->GetPayload()[1]);
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

	void ArrayTo32BitArray(uint8_t* incomingPayload)
	{
		ATUI_R.array[0] = incomingPayload[0];
		ATUI_R.array[1] = incomingPayload[1];
		ATUI_R.array[2] = incomingPayload[2];
		ATUI_R.array[3] = incomingPayload[3];
	}

	void LocalMACToPayload()
	{
		for (uint8_t i = 0; i < LOLA_LINK_INFO_MAC_LENGTH; i++)
		{
			PacketHolder.GetPayload()[1 + i] = LinkInfo->GetLocalMAC()[i];
		}
	}

	inline void PartnerMACToPayload()
	{
		for (uint8_t i = 0; i < LOLA_LINK_INFO_MAC_LENGTH; i++)
		{
			PacketHolder.GetPayload()[1 + i] = LinkInfo->GetPartnerMAC()[i];
		}
	}

	void PrepareLinkDiscovery()
	{
		PrepareLinkPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_LINK_DISCOVERY);
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
		PrepareLinkPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_HOST_BROADCAST);
		LocalMACToPayload();
	}

	void PrepareLinkRequest()								//Remote.
	{
		PrepareLinkPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_REMOTE_LINK_REQUEST);
		LocalMACToPayload();
	}

	void PrepareLinkRequestAccepted()						//Host.
	{
		PrepareLinkPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_HOST_LINK_ACCEPTED);
		PartnerMACToPayload();
	}

	void PrepareLinkRequestReady()							//Remote.
	{
		PrepareLinkPacket(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_REMOTE_LINK_READY);
		LocalMACToPayload();
	}

	void PrepareLinkConnectingSwitchOver()					//Host.
	{
		PrepareLinkPacketWithAck(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_ACK_LINK_REQUEST_SWITCHOVER);
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
		PrepareLinkPacketWithAck(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_ACK_PROTOCOL_SWITCHOVER);
	}

	void PrepareLinkInfoSyncUpdate(const uint8_t contentId, const uint32_t content) //Host
	{
		PacketHolder.SetDefinition(&LinkDefinition);
		PacketHolder.SetId(contentId);
		PacketHolder.GetPayload()[0] = LOLA_LINK_SUBHEADER_INFO_SYNC_UPDATE;

		ATUI_S.uint = content;
		ArrayToPayload();
	}

	void PrepareLinkInfoSyncAdvanceRequest(const uint8_t contentId)						//Both.
	{
		PrepareLinkPacketWithAck(contentId, LOLA_LINK_SUBHEADER_ACK_INFO_SYNC_ADVANCE);
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