// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h

#include <Services\Link\LoLaLinkDefinitions.h>

#include <Services\IPacketSendService.h>
#include <Services\LoLaServicesManager.h>
#include <LoLaLinkInfo.h>

#include <Callback.h>
#include <LoLaCrypto\TinyCRC.h>
//#include <LoLaCrypto\LoLaCryptoTokenSource.h>
#include <LoLaCrypto\LoLaCryptoKeyExchange.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>

#include <Services\Link\LoLaLinkClockSyncer.h>

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
	LinkedPacketDefinition				DefinitionLinked;
	PingPacketDefinition				DefinitionPing;

	UnlinkedShortPacketDefinition		DefinitionUnlinkedShort;
	UnlinkedLongWithAckPacketDefinition	DefinitionUnlinkedLongWithAck;
	UnlinkedLongPacketDefinition		DefinitionUnlinkedLong;

	uint32_t StateStartTime = ILOLA_INVALID_MILLIS;

	volatile uint32_t LastSent = ILOLA_INVALID_MILLIS;

	volatile bool ReportPending = false;

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

	ICryptoKeyExchanger*		KeyExchanger = nullptr;



	//Sub state.
	uint8_t LinkingState = 0;

	LoLaLinkInfo* LinkInfo = nullptr;

	////Synced Clock.
	LoLaLinkClockSyncer* ClockSyncerPointer = nullptr;
	IClockSyncTransaction* ClockSyncTransaction = nullptr;

	////Crypto Challenge.
	IChallengeTransaction* ChallengeTransaction = nullptr;

	//Optimized memory usage grunt packet.
	TemplateLoLaPacket<LOLA_LINK_SERVICE_PACKET_MAX_SIZE> PacketHolder;

	//Shared helper.
	uint32_t LinkingStart = 0;

	//Subservices.
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
	LoLaDiagnosticsService Diagnostics;
#endif

protected:
	///Common packet handling.
	virtual void OnLinkInfoSyncReceived(const uint16_t rtt, const uint8_t rssi) {}

	///Host packet handling.
	//Unlinked packets.
	virtual void OnLinkDiscoveryReceived() {}
	virtual void OnPKCRequestReceived(const uint8_t sessionId, const uint32_t remoteMACHash) {}
	virtual void OnEncodedPublicKeyReceived(const uint8_t sessionId, uint8_t * encodedPublicKey) {}
	virtual bool OnEncodedLinkRequestReceived(const uint8_t sessionId, uint32_t encodedMACHash) {}

	//Linked packets.
	//virtual void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	//virtual void OnChallengeResponseReceived(const uint8_t requestId, const uint32_t token) {}
	//virtual void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	///

	///Remote packet handling.
	//Unlinked packets.
	virtual void OnIdBroadcastReceived(const uint8_t sessionId, const uint32_t hostMACHash) {}
	virtual void OnPKCBroadcastReceived(const uint8_t sessionId, uint8_t* hostPublicKey) {}
	virtual void OnEncodedSharedKeyReceived(const uint8_t sessionId, uint8_t* encodedSharedKey) {}

	//Linked packets.
	//virtual void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	//virtual void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	//virtual void OnChallengeRequestReceived(const uint8_t requestId, const uint32_t token) {}
	//virtual void OnLinkingSwitchOverReceived(const uint8_t requestId, const uint8_t subHeader) {}
	///

	//Internal housekeeping.
	//virtual void SetBaseSeed() {}
	virtual void OnClearSession() {};
	virtual void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState) {}

	//Runtime handlers.
	virtual void OnLinking() { SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD); }
	virtual void OnAwaitingLink() { SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD); }
	//virtual bool OnChallenging() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); return false; }
	//virtual bool OnClockSync() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); return false; }
	//virtual bool OnInfoSync() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); return false; }
	//virtual void OnLinkProtocolSwitchOver() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }
	virtual void OnKeepingLink() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }


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
	void OnService()
	{
		switch (LinkInfo->GetLinkState())
		{
		case LoLaLinkInfo::LinkStateEnum::Setup:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
			OnAwaitingLink();
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_CANCEL)
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
			OnKeepingLinkCommon();
			break;
		case LoLaLinkInfo::LinkStateEnum::Disabled:
		default:
			Disable();
			return;
		}
	}



private:
	//inline void OnLinking()
	//{
	//	switch (LinkingState)
	//	{
	//	case LinkingStagesEnum::ChallengeStage:
	//		if (OnChallenging())
	//		{
	//			SetLinkingState(LinkingStagesEnum::ClockSyncStage);
	//		}
	//		break;
	//	case LinkingStagesEnum::InfoSyncStage:
	//		if (OnInfoSync())
	//		{
	//			SetLinkingState(LinkingStagesEnum::LinkProtocolSwitchOver);
	//		}
	//		break;
	//	case LinkingStagesEnum::ClockSyncStage:
	//		if (OnClockSync())
	//		{
	//			SetLinkingState(LinkingStagesEnum::LinkProtocolSwitchOver);
	//		}
	//		break;
	//	case LinkingStagesEnum::LinkProtocolSwitchOver:
	//		//We transition forward when we receive the exppected packet/ack.
	//		OnLinkProtocolSwitchOver();
	//		break;
	//	case LinkingStagesEnum::LinkingDone:
	//		//All connecting stages complete, we have a link.
	//		UpdateLinkState(LoLaLinkInfo::LinkStateEnum::Linked);
	//		break;
	//	default:
	//		UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
	//		SetNextRunASAP();
	//		break;
	//	}
	//}
//}

	inline void OnKeepingLinkCommon()
	{
		if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT)
		{
			//Link timed out.
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
		}
		else if (PowerBalancer.Update())
		{
			SetNextRunASAP();
		}
		else if (ReportPending) //Priority update, to keep link info up to date.
		{
			PrepareLinkInfoReport();
			RequestSendPacket();
		}
		else if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_LINKED_MAX_PANIC)
		{
			PowerBalancer.SetMaxPower();
			PreparePing(); 		//Send Panic Ping!
			RequestSendPacket();
		}
		else if (LinkInfo->GetLocalInfoUpdateRemotelyElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_UPDATE_PERIOD)
		{
			ReportPending = true; //Send link info update, deferred.
			SetNextRunASAP();
		}
		else
		{
			OnKeepingLink();
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



protected:
	void OnSendOk(const uint8_t header, const uint32_t sendDuration)
	{
		LastSent = millis();

		if (header == DefinitionLinked.GetHeader())
		{
			if (ReportPending)
			{
				ReportPending = false;
				LinkInfo->StampLocalInfoLastUpdatedRemotely();
			}
		}

		SetNextRunASAP();
	}

	void OnSendFailed()
	{
		//In case the send fails, this prevents from immediate resending.
		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	}

	void OnLinkInfoReportReceived(const bool requestUpdate, const uint8_t rssi)
	{
		if (LinkInfo->HasLink())
		{
			LinkInfo->SetPartnerRSSINormalized(rssi);
			LinkInfo->StampPartnerInfoUpdated();

			ReportPending = requestUpdate;
			SetNextRunASAP();
		}
	}

	void ResetStateStartTime()
	{
		StateStartTime = millis();
	}

	uint32_t GetElapsedSinceStateStart()
	{
		if (StateStartTime == ILOLA_INVALID_MILLIS)
		{
			return ILOLA_INVALID_MILLIS;
		}
		else
		{
			return millis() - StateStartTime;
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
			return millis() - LastSent;
		}
	}

	//void SetTOTPEnabled()
	//{
	//	//SetBaseSeed();
	//	//CryptoSeed.SetTOTPEnabled(true, GetLoLa()->GetClockSource(), ChallengeTransaction->GetToken());
	//}

	void SetLinkingState(const uint8_t linkingState)
	{
		LinkingState = linkingState;
		ResetLastSentTimeStamp();

		Serial.print(F("Awaiting Link:"));
		Serial.println(LinkingState);

		SetNextRunASAP();
	}

	void ResetLastSentTimeStamp()
	{
		LastSent = ILOLA_INVALID_MILLIS;
	}

	void TimeStampLastSent()
	{
		LastSent = millis();
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
		if (!packetMap->AddMapping(&DefinitionLinked) ||
			!packetMap->AddMapping(&DefinitionPing) ||
			!packetMap->AddMapping(&DefinitionUnlinkedShort) ||
			!packetMap->AddMapping(&DefinitionUnlinkedLong) ||
			!packetMap->AddMapping(&DefinitionUnlinkedLongWithAck))
		{
			return false;
		}

		//Make sure our re-usable packet has enough space for all our packets.
		if (Packet->GetMaxSize() < DefinitionLinked.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionPing.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionUnlinkedShort.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionUnlinkedLong.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionUnlinkedLongWithAck.GetTotalSize())
		{
			return false;
		}

		return true;
	}

	void ClearSession()
	{
		ReportPending = false;

		LastSent = ILOLA_INVALID_MILLIS;

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
			ClockSyncerPointer->Setup(GetLoLa()->GetClockSource()) &&
			KeyExchanger->Setup())
		{
			LinkInfo = ServicesManager->GetLinkInfo();

			if (LinkInfo != nullptr)
			{
				LinkInfo->SetDriver(GetLoLa());
				LinkInfo->Reset();

				//Make sure to lazy load the local MAC on startup.
				LinkInfo->GetLocalMACHash();

				if (PowerBalancer.Setup(GetLoLa(), LinkInfo))
				{
#ifdef USE_FREQUENCY_HOP
					FrequencyHopper.SetCryptoSeedSource(&CryptoSeed);
#endif

					ClearSession();

#ifdef DEBUG_LOLA
					Serial.print(F("Local MAC: "));
					LinkInfo->PrintMac(&Serial);
					Serial.println();
					Serial.print(F(" hash: "));
					Serial.println(LinkInfo->GetLocalMACHash());
#endif
					//GetLoLa()->GetCryptoSeed()->Reset();
					ResetStateStartTime();
					SetNextRunASAP();

					return true;
				}
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

		serial->print(F("RSSI :"));
		serial->println(LinkInfo->GetRSSINormalized());
		serial->print(F("RSSI Partner:"));
		serial->println(LinkInfo->GetPartnerRSSINormalized());

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
		else if (GetLoLa()->GetRejectedCount() > 0)
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
	}
#endif

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
				LinkInfo->Reset();
				PowerBalancer.SetMaxPower();
				ServicesManager->NotifyServicesLinkUpdated(false);
			}

			switch (newState)
			{
			case LoLaLinkInfo::LinkStateEnum::Setup:
				ClearSession();
				//GetLoLa()->GetCryptoSeed()->Reset();
				SetNextRunDefault();
#ifdef USE_FREQUENCY_HOP
				FrequencyHopper.ResetChannel();
#else
				GetLoLa()->SetChannel(0);
#endif
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
				//GetLoLa()->GetCryptoSeed();
				SetLinkingState(0);
				PowerBalancer.SetMaxPower();
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
				//GetLoLa()->GetCryptoSeed()->Reset();
				//Sleep time is set on Host/Remote
				break;
			case LoLaLinkInfo::LinkStateEnum::Linking:

				//SetBaseSeed(); //TODO: Update time seed usage.
				////TODO: Enable global crypto for all packets.

				SetLinkingState(0);
				LinkingStart = millis();
#ifdef DEBUG_LOLA				
				Serial.print(F("Linking to MAC Hash: "));
				Serial.println(LinkInfo->GetPartnerMACHash());
				Serial.print(F("-Session: "));
				Serial.println(LinkInfo->GetSessionId());
#endif
				//SetNextRunASAP();
				SetNextRunLong();
				break;
			case LoLaLinkInfo::LinkStateEnum::Linked:
				LinkInfo->StampLinkStarted();
				GetLoLa()->ResetStatistics();
				//SetTOTPEnabled();
				SetNextRunASAP();
				ClockSyncerPointer->StampSynced();
				PowerBalancer.SetMaxPower();

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
				Serial.println(MillisSync());
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

			OnLinkStateChanged(newState);
			LinkInfo->UpdateState(newState);
		}
	}


	uint32_t GetElapsedLastValidReceived()
	{
		if (GetLoLa()->GetLastValidReceivedMillis() != ILOLA_INVALID_MILLIS)
		{
			return millis() - GetLoLa()->GetLastValidReceivedMillis();
		}
		else
		{
			return UINT32_MAX;
		}
	}

	uint32_t GetElapsedLastSent()
	{
		if (GetLoLa()->GetLastValidSentMillis() != ILOLA_INVALID_MILLIS)
		{
			return millis() - GetLoLa()->GetLastValidSentMillis();
		}
		else
		{
			return 0;
		}
	}

	bool ProcessPacket(ILoLaPacket* receivedPacket)
	{
		//Switch the packet to the appropriate method.
		switch (receivedPacket->GetDataHeader())
		{
		case LOLA_LINK_HEADER_LINKED:
			switch (receivedPacket->GetPayload()[0])
			{
				//To Host.
			//case LOLA_LINK_SUBHEADER_INFO_SYNC:
			//	ATUI_R.uint = incomingPacket->GetPayload()[2] << 8;
			//	ATUI_R.uint += incomingPacket->GetPayload()[3];
			//	OnLinkInfoSyncReceived(incomingPacket->GetPayload()[0], (uint16_t)ATUI_R.uint);
			//	break;
			//case LOLA_LINK_SUBHEADER_LINK_INFO_REPORT:
			//	OnLinkInfoReportReceived(incomingPacket->GetId(), incomingPacket->GetPayload()[1]);
			//	break;

			//case LOLA_LINK_SUBHEADER_CHALLENGE_REPLY:
			//	ArrayToR_Array(&incomingPacket->GetPayload()[1]);
			//	OnChallengeResponseReceived(incomingPacket->GetId(), ATUI_R.uint);
			//	break;
			//case LOLA_LINK_SUBHEADER_NTP_REQUEST:
			//	ArrayToR_Array(&incomingPacket->GetPayload()[1]);
			//	OnClockSyncRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
			//	break;
			//case LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST:
			//	ArrayToR_Array(&incomingPacket->GetPayload()[1]);
			//	OnClockSyncTuneRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
			//	break;

			//	//To Remote.
			//case LOLA_LINK_SUBHEADER_NTP_REPLY:
			//	ArrayToR_Array(&incomingPacket->GetPayload()[1]);
			//	OnClockSyncResponseReceived(incomingPacket->GetId(), ATUI_R.iint);
			//	break;
			//case LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY:
			//	ArrayToR_Array(&incomingPacket->GetPayload()[1]);
			//	OnClockSyncTuneResponseReceived(incomingPacket->GetId(), ATUI_R.iint);
			//	break;
			//case LOLA_LINK_SUBHEADER_CHALLENGE_REQUEST:
			//	ArrayToR_Array(&incomingPacket->GetPayload()[1]);
			//	OnChallengeRequestReceived(incomingPacket->GetId(), ATUI_R.uint);
			//	break;
			default:
				break;
			}
			break;

		case LOLA_LINK_HEADER_UNLINKED_SHORT_HEADER:
			switch (receivedPacket->GetPayload()[0])
			{
				//To Host.
			case LOLA_LINK_SUBHEADER_LINK_DISCOVERY:
				OnLinkDiscoveryReceived();
				break;
			case LOLA_LINK_SUBHEADER_REMOTE_PKC_START_REQUEST:
				ArrayToR_Array(&receivedPacket->GetPayload()[1]);
				OnPKCRequestReceived(receivedPacket->GetId(), ATUI_R.uint);
				break;

				//To remote.
			case LOLA_LINK_SUBHEADER_HOST_ID_BROADCAST:
				ArrayToR_Array(&receivedPacket->GetPayload()[1]);
				OnIdBroadcastReceived(receivedPacket->GetId(), ATUI_R.uint);
				break;
			}
			break;
		case LOLA_LINK_HEADER_UNLINKED_LONG_HEADER:
			switch (receivedPacket->GetPayload()[0])
			{
				//To Host.
			case LOLA_LINK_SUBHEADER_REMOTE_PKC_PUBLIC_ENCODED:
				OnEncodedPublicKeyReceived(receivedPacket->GetId(), &receivedPacket->GetPayload()[1]);
				break;

				//To remote.
			case LOLA_LINK_SUBHEADER_HOST_PKC_BROADCAST:
				OnPKCBroadcastReceived(receivedPacket->GetId(), &receivedPacket->GetPayload()[1]);
				break;
			case LOLA_LINK_SUBHEADER_HOST_SHARED_KEY_PUBLIC_ENCODED:
				OnEncodedSharedKeyReceived(receivedPacket->GetId(), &receivedPacket->GetPayload()[1]);
				break;
			}
			break;
		default:
			return false;
		}

		return true;
	}

	bool ProcessAckedPacket(ILoLaPacket* receivedPacket)
	{
		if (LinkInfo->HasLink())
		{
			if (receivedPacket->GetDataHeader() == LOLA_LINK_HEADER_LINKED_WITH_ACK)
			{
				return true;//Ping packet.
			}
		}
		else
		{
			switch (receivedPacket->GetDataHeader())
			{
			case LOLA_LINK_HEADER_UNLINKED_LONG_WITH_ACK_HEADER:
				//To host.
				ArrayToR_Array(&receivedPacket->GetPayload()[1]);
				return OnEncodedLinkRequestReceived(receivedPacket->GetId(), ATUI_R.uint);
			default:
				break;
			}
		}

		return false;
	}

	///Packet builders.
	void PreparePing()
	{
		PacketHolder.SetDefinition(&DefinitionPing);
		PacketHolder.SetId(random(0, UINT8_MAX));
	}

	/////Linking time packets.
	//void PrepareChallengeRequest()							//Host.
	//{
	//	PrepareLinkedPacket(ChallengeTransaction->GetTransactionId(), LOLA_LINK_SUBHEADER_CHALLENGE_REQUEST);
	//	ATUI_S.uint = ChallengeTransaction->GetToken();
	//	ArrayToPayload();
	//}

	//void PrepareChallengeReply()							//Remote.
	//{
	//	PrepareLinkedPacket(ChallengeTransaction->GetTransactionId(), LOLA_LINK_SUBHEADER_CHALLENGE_REPLY);
	//	ATUI_S.uint = ChallengeTransaction->GetToken();
	//	ArrayToPayload();
	//}

	//void PrepareClockSyncRequest(const uint8_t requestId)	//Remote
	//{
	//	PrepareLinkedPacket(requestId, LOLA_LINK_SUBHEADER_NTP_REQUEST);
	//	//Rest of Payload is set on OnPreSend.
	//}

	//void PrepareClockSyncResponse(const uint8_t requestId, const uint32_t estimationError)
	//{
	//	PrepareLinkedPacket(requestId, LOLA_LINK_SUBHEADER_NTP_REPLY);
	//	ATUI_S.uint = estimationError;
	//	ArrayToPayload();
	//}

	void PrepareClockSyncTuneRequest(const uint8_t requestId)	//Remote
	{
		PrepareLinkedPacket(requestId, LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST);
		//Rest of Payload is set on OnPreSend.
	}

	void PrepareClockSyncTuneResponse(const uint8_t requestId, const uint32_t estimationError)
	{
		PrepareLinkedPacket(requestId, LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY);
		ATUI_S.uint = estimationError;
		S_ArrayToPayload();
	}

	//void PrepareLinkProtocolSwitchOver()					//Host.
	//{
	//	PrepareLinkedPacketWithAck(LinkInfo->GetSessionId(), LOLA_LINK_SUBHEADER_ACK_PROTOCOL_SWITCHOVER);
	//}

	void PrepareLinkInfoSyncUpdate(const uint16_t rtt, const uint8_t rssi) //Both
	{
		PrepareLinkedPacket(0, LOLA_LINK_SUBHEADER_INFO_SYNC);//Ignore id.
		PacketHolder.GetPayload()[1] = rssi;
		PacketHolder.GetPayload()[2] = rtt & 0xFF; //MSB 16 bit unsigned.
		PacketHolder.GetPayload()[3] = (rtt >> 8) & 0xFF;
		PacketHolder.GetPayload()[4] = UINT8_MAX; //Padding
	}

	void PrepareLinkInfoReport()
	{
		PrepareLinkedPacket(LinkInfo->GetPartnerInfoUpdateElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_STALE_PERIOD, LOLA_LINK_SUBHEADER_LINK_INFO_REPORT, 4);
		PacketHolder.GetPayload()[1] = LinkInfo->GetRSSINormalized();
	}


	inline void PrepareLinkedPacket(const uint8_t requestId, const uint8_t subHeader, const uint8_t paddingSize = 0)
	{
		PacketHolder.SetDefinition(&DefinitionLinked);
		PacketHolder.GetPayload()[0] = subHeader;
		if (paddingSize > 0)
		{
			for (uint8_t i = 1; i < min(DefinitionLinked.GetPayloadSize(), paddingSize + 1); i++)
			{
				PacketHolder.GetPayload()[i] = UINT8_MAX;
			}
		}
	}

	inline void PrepareUnlinkedShortPacket(const uint8_t requestId, const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&DefinitionUnlinkedShort);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = subHeader;
	}

	inline void PrepareUnlinkedLongPacket(const uint8_t requestId, const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&DefinitionUnlinkedLong);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = subHeader;
	}

	inline void PrepareUnlinkedLongPacketWithAck(const uint8_t requestId)
	{
		PacketHolder.SetDefinition(&DefinitionUnlinkedLongWithAck);
		PacketHolder.SetId(requestId);
	}

	inline void S_ArrayToPayload()
	{
		PacketHolder.GetPayload()[1] = ATUI_S.array[0];
		PacketHolder.GetPayload()[2] = ATUI_S.array[1];
		PacketHolder.GetPayload()[3] = ATUI_S.array[2];
		PacketHolder.GetPayload()[4] = ATUI_S.array[3];
	}

	inline void ArrayToR_Array(uint8_t* incomingPayload)
	{
		ATUI_R.array[0] = incomingPayload[0];
		ATUI_R.array[1] = incomingPayload[1];
		ATUI_R.array[2] = incomingPayload[2];
		ATUI_R.array[3] = incomingPayload[3];
	}
};
#endif