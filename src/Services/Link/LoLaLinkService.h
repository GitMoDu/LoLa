// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h

#include <Services\Link\LoLaLinkDefinitions.h>

#include <Services\IPacketSendService.h>
#include <Services\LoLaServicesManager.h>
#include <LoLaLinkInfo.h>

#include <Callback.h>
#include <LoLaCrypto\TinyCRC.h>
#include <LoLaCrypto\LoLaCryptoKeyExchange.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>
#include <LoLaCrypto\LoLaCryptoTokenSource.h>


#include <Services\Link\LoLaLinkClockSyncer.h>
#include <Services\Link\LoLaLinkPowerBalancer.h>
#include <Services\Link\LoLaLinkChannelManager.h>

#include <Services\Link\LoLaLinkTimedHopper.h>

#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
#include <Services\LoLaDiagnosticsService\LoLaDiagnosticsService.h>
#endif

class LoLaLinkService : public IPacketSendService
{
private:
	PingPacketDefinition				DefinitionPing;
	LinkReportPacketDefinition			DefinitionReport;

	LinkShortPacketDefinition			DefinitionShort;
	LinkShortWithAckPacketDefinition	DefinitionShortWithAck;
	LinkLongPacketDefinition			DefinitionLong;

	uint32_t StateStartTime = ILOLA_INVALID_MILLIS;

	volatile uint32_t LastSent = ILOLA_INVALID_MILLIS;

	volatile bool ReportPending = false;

	LoLaServicesManager * ServicesManager = nullptr;

	//Sub-services.
	LoLaLinkTimedHopper TimedHopper;

	//Channel management.
	LoLaLinkChannelManager ChannelManager;

	//Power balancer.
	LoLaLinkPowerBalancer PowerBalancer;

	//Crypto Token.
	ITokenSource* CryptoToken = nullptr;

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

	///Crypto key exchanger.
	LoLaCryptoKeyExchanger	KeyExchanger;
	uint32_t KeysLastGenerated = ILOLA_INVALID_MILLIS;
	///


	//Shared Sub state helpers.
	uint8_t LinkingState = 0;
	uint8_t InfoSyncStage = 0;

	LoLaLinkInfo* LinkInfo = nullptr;

	////Synced Clock.
	LoLaLinkClockSyncer* ClockSyncerPointer = nullptr;
	IClockSyncTransaction* ClockSyncTransaction = nullptr;

	//Optimized memory usage grunt packet.
	TemplateLoLaPacket<LOLA_LINK_SERVICE_PACKET_MAX_SIZE> PacketHolder;

	//Shared helper.
	uint32_t SubStateStart = ILOLA_INVALID_MILLIS;

	//Subservices.
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
	LoLaDiagnosticsService Diagnostics;
#endif

#ifdef DEBUG_LOLA
	uint32_t LinkingDuration = 0;
	uint32_t PKCDuration = 0;
#endif

protected:
	///Common packet handling.
	virtual void OnLinkAckReceived(const uint8_t header, const uint8_t id) {}
	virtual void OnPingAckReceived(const uint8_t id) {}

	///Host packet handling.
	//Unlinked packets.
	virtual void OnLinkDiscoveryReceived() {}
	virtual void OnPKCRequestReceived(const uint8_t sessionId, const uint32_t remoteMACHash) {}
	virtual void OnRemotePublicKeyReceived(const uint8_t sessionId, uint8_t * encodedPublicKey) {}

	//Linked packets.
	virtual void OnRemoteInfoSyncReceived(const uint8_t rssi) {}
	virtual void OnHostInfoSyncRequestReceived() {}
	virtual void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	virtual void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMillis) {}
	///

	///Remote packet handling.
	//Unlinked packets.
	virtual void OnIdBroadcastReceived(const uint8_t sessionId, const uint32_t hostMACHash) {}
	virtual void OnHostPublicKeyReceived(const uint8_t sessionId, uint8_t* hostPublicKey) {}
	virtual bool OnAckedPacketReceived(ILoLaPacket* receivedPacket) { return false; }

	//Linked packets.
	virtual void OnHostInfoSyncReceived(const uint8_t rssi, const uint16_t rtt) {}
	virtual void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	virtual void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedError) {}
	///

	//Internal housekeeping.
	virtual void OnClearSession() {};
	virtual void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState) {}

	//Runtime handlers.
	virtual void OnLinking() { SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD); }
	virtual bool OnAwaitingLink() { return false; }
	virtual void OnKeepingLink() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }


public:
	LoLaLinkService(Scheduler* scheduler, ILoLa* loLa)
		: IPacketSendService(scheduler, LOLA_LINK_SERVICE_CHECK_PERIOD, loLa, &PacketHolder)
		, TimedHopper(scheduler, loLa)
#ifdef LOLA_LINK_DIAGNOSTICS_ENABLED
		, Diagnostics(scheduler, loLa, &LinkInfo)
#endif
	{
	}

	bool SetServicesManager(LoLaServicesManager * servicesManager)
	{
		ServicesManager = servicesManager;

		if (ServicesManager != nullptr)
		{
			return ServicesManager->Add(&TimedHopper);
		}
		return false;
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
			if (KeysLastGenerated == ILOLA_INVALID_MILLIS || millis() - KeysLastGenerated > LOLA_LINK_SERVICE_UNLINK_KEY_PAIR_LIFETIME)
			{
#ifdef DEBUG_LOLA_CRYPTO
				uint32_t SharedKeyTime = micros();
#endif
				KeyExchanger.GenerateNewKeyPair();
				KeysLastGenerated = millis();
#ifdef DEBUG_LOLA
#ifdef DEBUG_LOLA_CRYPTO
				SharedKeyTime = micros() - SharedKeyTime;
				Serial.print(F("Keys generation took "));
				Serial.print(SharedKeyTime);
				Serial.println(F(" us."));
#endif
#endif
				SetNextRunASAP();
			}
			else
			{
				if (!OnAwaitingLink())//Time out is different for host/remote.
				{
					UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingSleeping);
				}
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			break;
		case LoLaLinkInfo::LinkStateEnum::Linking:
			if (GetElapsedSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_LINKING_CANCEL)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			}
			else
			{
				OnLinking();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
			if (GetElapsedLastValidReceived() > LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			}
			else
			{
				OnKeepingLinkCommon();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Disabled:
		default:
			Disable();
			return;
		}
	}

private:
	inline void OnKeepingLinkCommon()
	{
		if (PowerBalancer.Update())
		{
			SetNextRunASAP();
		}
		else if (ReportPending) //Priority update, to keep link info up to date.
		{
			PrepareLinkReport(LinkInfo->GetPartnerLastReportElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_STALE_PERIOD);
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

		if (header == LOLA_LINK_HEADER_REPORT &&
			LinkInfo->HasLink() &&
			ReportPending)
		{
			ReportPending = false;
			LinkInfo->StampLocalInfoLastUpdatedRemotely();
		}

		SetNextRunASAP();
	}

	void OnSendFailed()
	{
		//In case the send fails, this prevents from immediate resending.
		SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
	}

	void OnLinkInfoReportReceived(const uint8_t rssi, const uint32_t partnerReceivedCount)
	{
		if (LinkInfo->HasLink())
		{
			LinkInfo->StampPartnerInfoUpdated();
			LinkInfo->SetPartnerRSSINormalized(rssi);
			LinkInfo->SetPartnerReceivedCount(partnerReceivedCount);

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

	void SetLinkingState(const uint8_t linkingState)
	{
		LinkingState = linkingState;
		ResetLastSentTimeStamp();

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
		if (!packetMap->AddMapping(&DefinitionPing) ||
			!packetMap->AddMapping(&DefinitionReport) ||
			!packetMap->AddMapping(&DefinitionShort) ||
			!packetMap->AddMapping(&DefinitionShortWithAck) ||
			!packetMap->AddMapping(&DefinitionLong))
		{
			return false;
		}

		//Make sure our re-usable packet has enough space for all our packets.
		if (Packet->GetMaxSize() < DefinitionPing.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionReport.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionShort.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionShortWithAck.GetTotalSize() ||
			Packet->GetMaxSize() < DefinitionLong.GetTotalSize())
		{
			return false;
		}

		return true;
	}

	void ClearSession()
	{
		ReportPending = false;

		if (ClockSyncerPointer != nullptr)
		{
			ClockSyncerPointer->Reset();
		}

		if (ClockSyncTransaction != nullptr)
		{
			ClockSyncTransaction->Reset();
		}

		if (LinkInfo != nullptr)
		{
			LinkInfo->Reset();
		}

		GetLoLa()->GetCryptoEncoder()->Clear();
		CryptoToken->SetSeed(0);

		KeyExchanger.ClearPartner();

		InfoSyncStage = 0;

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
			ServicesManager != nullptr &&
			KeyExchanger.Setup() &&
			ClockSyncerPointer->Setup(GetLoLa()->GetClockSource()) &&
			ChannelManager.Setup(GetLoLa()) &&
			TimedHopper.Setup(GetLoLa()->GetClockSource()))
		{
			LinkInfo = ServicesManager->GetLinkInfo();
			CryptoToken = TimedHopper.GetTokenSource();

			if (LinkInfo != nullptr && CryptoToken != nullptr)
			{
				LinkInfo->SetDriver(GetLoLa());
				LinkInfo->Reset();
				KeysLastGenerated = ILOLA_INVALID_MILLIS;

				//Make sure to lazy load the local MAC on startup.
				LinkInfo->GetLocalId();

				if (PowerBalancer.Setup(GetLoLa(), LinkInfo))
				{
#ifdef USE_FREQUENCY_HOP
					//FrequencyHopper.SetCryptoSeedSource(&CryptoSeed);
#endif
					ClearSession();
#ifdef DEBUG_LOLA
					Serial.print(F("Local MAC: "));
					LinkInfo->PrintMac(&Serial);
					Serial.println();
					Serial.print(F("\tId: "));
					Serial.println(LinkInfo->GetLocalId());
#endif
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

		if (AliveSeconds / 86400 > 0)
		{
			serial->print(AliveSeconds / 86400);
			serial->print(F("d "));
			AliveSeconds = AliveSeconds % 86400;
		}

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
		serial->print(F("Partner Got: "));
		serial->println(LinkInfo->GetPartnerReceivedCount());
		serial->print(F("Lost: "));
		serial->println(max(LinkInfo->GetPartnerReceivedCount(), GetLoLa()->GetSentCount()) - LinkInfo->GetPartnerReceivedCount());
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
		serial->print(F("Timming Collision: "));
		serial->println(GetLoLa()->GetTimingCollisionCount());

		serial->print(F("ClockSync adjustments: "));
		serial->println(LinkInfo->GetClockSyncAdjustments());
		serial->println();
	}
#endif

	void UpdateLinkState(const LoLaLinkInfo::LinkStateEnum newState)
	{
		if (LinkInfo->GetLinkState() != newState)
		{
#ifdef DEBUG_LOLA
			if (newState == LoLaLinkInfo::LinkStateEnum::Linked)
			{
				LinkingDuration = GetElapsedSinceStateStart();
			}
#endif
			ResetStateStartTime();
			ResetLastSentTimeStamp();

			//Previous state.
			if (LinkInfo->GetLinkState() == LoLaLinkInfo::LinkStateEnum::Linked)
			{
#ifdef DEBUG_LOLA
				DebugLinkStatistics(&Serial);
#endif
				//Notify all link dependent services to stop.
				ClearSession();
				PowerBalancer.SetMaxPower();
				ChannelManager.ResetChannel();
				GetLoLa()->OnStart();
				ServicesManager->NotifyServicesLinkUpdated(false);
			}

			switch (newState)
			{
			case LoLaLinkInfo::LinkStateEnum::Setup:
				GetLoLa()->Enable();
				SetLinkingState(0);
				ClearSession();
				PowerBalancer.SetMaxPower();
				ChannelManager.ResetChannel();
				GetLoLa()->OnStart();
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
				ClearSession();
				SetLinkingState(0);
				PowerBalancer.SetMaxPower();
				ChannelManager.ResetChannel();
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
				ClearSession();
				SetLinkingState(0);
				PowerBalancer.SetMaxPower();
				ChannelManager.ResetChannel();
				GetLoLa()->OnStart();
				//Sleep time is set on Host/Remote virtual.
				break;
			case LoLaLinkInfo::LinkStateEnum::Linking:
				SetLinkingState(0);
				CryptoToken->SetSeed(GetLoLa()->GetCryptoEncoder()->GetSeed());
				GetLoLa()->GetCryptoEncoder()->SetEnabled();
				PowerBalancer.SetMaxPower();
#ifdef DEBUG_LOLA				
				Serial.print(F("Linking to Id: "));
				Serial.println(LinkInfo->GetPartnerId());
				Serial.print(F("\tSession: "));
				Serial.println(LinkInfo->GetSessionId());
				Serial.print(F("PKC took "));
				Serial.print(PKCDuration);
				Serial.println(F(" ms."));
#endif
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::Linked:
				LinkInfo->StampLinkStarted();
				ClockSyncerPointer->StampSynced();
				GetLoLa()->ResetStatistics();
				PowerBalancer.SetMaxPower();
#ifdef DEBUG_LOLA
				Serial.println();
				Serial.print(F("Linked: "));
				Serial.println(MillisSync());
#ifdef LOLA_USE_ENCRYPTION
				Serial.print(F("Link secured with "));
				KeyExchanger.Debug(&Serial);
				Serial.println();
				Serial.print(F("\tEncrypted with "));
				GetLoLa()->GetCryptoEncoder()->Debug(&Serial);
				Serial.println();
#endif
				Serial.print(F("Linking took "));
				Serial.print(LinkingDuration);
				Serial.println(F(" ms."));
				Serial.print(F("Round Trip Time: "));
				Serial.print(LinkInfo->GetRTT());
				Serial.println(F(" us"));
				Serial.print(F("Estimated Latency: "));
				Serial.print((float)LinkInfo->GetRTT() / (float)2000, 2);
				Serial.println(F(" ms"));
				Serial.print(F("Latency compensation: "));
				Serial.print(GetLoLa()->GetETTM());
				Serial.println(F(" ms"));				
#endif

				//Notify all link dependent services they can start.
				ServicesManager->NotifyServicesLinkUpdated(true);
				SetNextRunASAP();
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
		return millis() - GetLoLa()->GetLastValidReceivedMillis();
	}

	uint32_t GetElapsedLastSent()
	{
		return millis() - GetLoLa()->GetLastValidSentMillis();
	}

	bool ProcessPacket(ILoLaPacket* receivedPacket)
	{
		//Switch the packet to the appropriate method.
		switch (receivedPacket->GetDataHeader())
		{
		case LOLA_LINK_HEADER_SHORT:
			switch (receivedPacket->GetPayload()[0])
			{
				///Unlinked packets.
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
				///

				///Linking Packets
			case LOLA_LINK_SUBHEADER_NTP_REQUEST:
				ArrayToR_Array(&receivedPacket->GetPayload()[1]);
				OnClockSyncRequestReceived(receivedPacket->GetId(), ATUI_R.uint);
				break;

				//To Remote.
			case LOLA_LINK_SUBHEADER_NTP_REPLY:
				ArrayToR_Array(&receivedPacket->GetPayload()[1]);
				OnClockSyncResponseReceived(receivedPacket->GetId(), ATUI_R.iint);
				break;
				///

				///Linked packets.
				//Host.
			case LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST:
				ArrayToR_Array(&receivedPacket->GetPayload()[1]);
				OnClockSyncTuneRequestReceived(receivedPacket->GetId(), ATUI_R.uint);
				break;

				//Remote.
			case LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY:
				ArrayToR_Array(&receivedPacket->GetPayload()[1]);
				OnClockSyncTuneResponseReceived(receivedPacket->GetId(), ATUI_R.iint);
				break;
				///
			default:
				break;
			}
			return true;
		case LOLA_LINK_HEADER_LONG:
			switch (receivedPacket->GetPayload()[0])
			{
				//To Host.
			case LOLA_LINK_SUBHEADER_REMOTE_PUBLIC_KEY:
				OnRemotePublicKeyReceived(receivedPacket->GetId(), &receivedPacket->GetPayload()[1]);
				break;

				//To remote.
			case LOLA_LINK_SUBHEADER_HOST_PUBLIC_KEY:
				OnHostPublicKeyReceived(receivedPacket->GetId(), &receivedPacket->GetPayload()[1]);
				break;
			}
			return true;
		case LOLA_LINK_HEADER_REPORT:
			switch (receivedPacket->GetId())
			{
				//To both.
			case LOLA_LINK_SUBHEADER_LINK_REPORT_WITH_REPLY:
				ReportPending = true;
			case LOLA_LINK_SUBHEADER_LINK_REPORT:
				ArrayToR_Array(&receivedPacket->GetPayload()[1]);
				OnLinkInfoReportReceived(receivedPacket->GetPayload()[0], ATUI_R.uint);
				break;

				//To Host.
			case LOLA_LINK_SUBHEADER_INFO_SYNC_HOST:
				ATUI_R.uint = receivedPacket->GetPayload()[1];
				ATUI_R.uint += receivedPacket->GetPayload()[2] << 8;
				OnHostInfoSyncReceived(receivedPacket->GetPayload()[0], (uint16_t)ATUI_R.uint);
				break;

				//To Remote.
			case LOLA_LINK_SUBHEADER_INFO_SYNC_REMOTE:
				OnRemoteInfoSyncReceived(receivedPacket->GetPayload()[0]);
				break;
			case LOLA_LINK_SUBHEADER_INFO_SYNC_REQUEST:
				OnHostInfoSyncRequestReceived();
				break;
			default:
				break;
			}
			return true;
		default:
			break;
		}

		return false;
	}

	bool ProcessAckedPacket(ILoLaPacket* receivedPacket)
	{
		if (receivedPacket->GetDataHeader() == LOLA_LINK_HEADER_PING_WITH_ACK)
		{
			switch (LinkInfo->GetLinkState())
			{
			case LoLaLinkInfo::LinkStateEnum::Linking:
				return true;
			case LoLaLinkInfo::LinkStateEnum::Linked:
				return true;
			default:
				return false;
			}
		}

		return OnAckedPacketReceived(receivedPacket);
	}

	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		if (header == LOLA_LINK_HEADER_PING_WITH_ACK)
		{
			OnPingAckReceived(id);
		}
		else
		{
			OnLinkAckReceived(header, id);
		}
	}

	///Packet builders.
	void PreparePing()
	{
		PacketHolder.SetDefinition(&DefinitionPing);
		PacketHolder.SetId(random(0, UINT8_MAX));
	}

	void PreparePublicKeyPacket(const uint8_t subHeader)
	{
		PrepareLongPacket(LinkInfo->GetSessionId(), subHeader);

		if (!KeyExchanger.GetPublicKeyCompressed(&PacketHolder.GetPayload()[1]))
		{
#ifdef DEBUG_LOLA
			Serial.println(F("Unable to read PK"));
#endif
		}
	}

	/////Linking time packets.
	void PrepareLinkReport(const bool requestReply)
	{
		if (requestReply)
		{
			PrepareReportPacket(LOLA_LINK_SUBHEADER_LINK_REPORT_WITH_REPLY);
		}
		else
		{
			PrepareReportPacket(LOLA_LINK_SUBHEADER_LINK_REPORT);
		}

		PacketHolder.GetPayload()[0] = LinkInfo->GetRSSINormalized();
		ATUI_S.uint = GetLoLa()->GetReceivedCount();
		S_ArrayToPayload();
	}

	inline void PrepareReportPacket(const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&DefinitionReport);
		PacketHolder.SetId(subHeader);
	}

	inline void PrepareShortPacket(const uint8_t requestId, const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&DefinitionShort);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = subHeader;
	}

	inline void PrepareLongPacket(const uint8_t requestId, const uint8_t subHeader)
	{
		PacketHolder.SetDefinition(&DefinitionLong);
		PacketHolder.SetId(requestId);
		PacketHolder.GetPayload()[0] = subHeader;
	}

	inline void PrepareShortPacketWithAck(const uint8_t requestId)
	{
		PacketHolder.SetDefinition(&DefinitionShortWithAck);
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