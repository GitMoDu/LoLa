// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h


#include <Services\Link\AbstractLinkService.h>


#include <Services\Link\LoLaLinkClockSyncer.h>

#include <LoLaCrypto\LoLaCryptoKeyExchange.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>

#include <Services\Link\LoLaLinkPowerBalancer.h>
#include <Services\Link\LoLaLinkChannelManager.h>

#include <Services\Link\LoLaLinkTimedHopper.h>

class LoLaLinkService : public AbstractLinkService
{
#ifdef DEBUG_LOLA
protected:
	uint32_t LinkingDuration = 0;
	uint32_t PKCDuration = 0;

private:
	uint32_t NextDebug = 0;
#define LOLA_LINK_DEBUG_UPDATE_MILLIS					((uint32_t)1000*(uint32_t)LOLA_LINK_DEBUG_UPDATE_SECONDS)

#else
private:
#endif

	//Sub-services.
	LoLaLinkTimedHopper TimedHopper;

	//Channel management.
	LoLaLinkChannelManager ChannelManager;

	//Power balancer.
	LoLaLinkPowerBalancer PowerBalancer;

	//Crypto Token.
	ITokenSource* CryptoToken = nullptr;

	//Link report tracking.
	bool ReportPending = false;

protected:
	//Crypto key exchanger.
	LoLaCryptoKeyExchanger	KeyExchanger;
	uint32_t KeysLastGenerated = ILOLA_INVALID_MILLIS;
	//

	//Synced Clock.
	LoLaLinkClockSyncer* ClockSyncerPointer = nullptr;
	IClockSyncTransaction* ClockSyncTransaction = nullptr;

	//Shared Sub state helpers.
	uint32_t SubStateStart = ILOLA_INVALID_MILLIS;
	uint8_t LinkingState = 0;
	uint8_t InfoSyncStage = 0;

protected:
	///Host packet handling.
	//Unlinked packets.
	virtual void OnLinkDiscoveryReceived() {}
	virtual void OnPKCRequestReceived(const uint8_t sessionId, const uint32_t remoteMACHash) {}
	virtual void OnRemotePublicKeyReceived(const uint8_t sessionId, uint8_t * encodedPublicKey) {}

	//Linked packets.
	virtual void OnRemoteInfoSyncReceived(const uint8_t rssi) {}
	virtual void OnHostInfoSyncRequestReceived() {}
	virtual void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMicros) {}
	virtual void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMicros) {}
	///

	///Remote packet handling.
	//Unlinked packets.
	virtual void OnIdBroadcastReceived(const uint8_t sessionId, const uint32_t hostMACHash) {}
	virtual void OnHostPublicKeyReceived(const uint8_t sessionId, uint8_t* hostPublicKey) {}

	//Linked packets.
	virtual void OnHostInfoSyncReceived(const uint8_t rssi, const uint16_t rtt) {}
	virtual void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedErrorMicros) {}
	virtual void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedErrorMicros) {}
	///

	//Internal housekeeping.
	virtual void OnClearSession() {};
	virtual void OnLinkStateChanged(const LoLaLinkInfo::LinkStateEnum newState) {}

	//Runtime handlers.
	virtual void OnLinking() { SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD); }
	virtual bool OnAwaitingLink() { return false; }
	virtual void OnKeepingLink() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }


public:
	LoLaLinkService(Scheduler* servicesScheduler, Scheduler* driverScheduler, ILoLaDriver* driver)
		: AbstractLinkService(servicesScheduler, driver)
		, TimedHopper(driverScheduler, driver)
	{
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
	bool OnAddSubServices()
	{
		return ServicesManager->Add(&TimedHopper);
	}

	bool OnSetup()
	{
		if (IPacketSendService::OnSetup() &&
			ClockSyncTransaction != nullptr &&
			ClockSyncerPointer != nullptr &&
			ServicesManager != nullptr &&
			KeyExchanger.Setup() &&
			ClockSyncerPointer->Setup(LoLaDriver->GetClockSource()) &&
			ChannelManager.Setup(LoLaDriver) &&
			TimedHopper.Setup(LoLaDriver->GetClockSource(), &ChannelManager))
		{
			LinkInfo = ServicesManager->GetLinkInfo();
			CryptoToken = TimedHopper.GetTokenSource();

			if (LinkInfo != nullptr && CryptoToken != nullptr)
			{
				LinkInfo->SetDriver(LoLaDriver);
				LinkInfo->Reset();
				KeysLastGenerated = ILOLA_INVALID_MILLIS;

				//Make sure to lazy load the local MAC on startup.
				LinkInfo->GetLocalId();

				if (PowerBalancer.Setup(LoLaDriver, LinkInfo))
				{
					ClearSession();
#ifdef DEBUG_LOLA
					Serial.print(F("Local MAC: "));
					LinkInfo->PrintMac(&Serial);
					Serial.println();
					Serial.print(F("\tId: "));
					Serial.println(LinkInfo->GetLocalId());

#ifdef LOLA_LINK_USE_ENCRYPTION
					Serial.print(F("Link secured with 160 bit "));
					KeyExchanger.Debug(&Serial);
					Serial.println();
					Serial.print(F("\tEncrypted with 128 bit cypher "));
					LoLaDriver->GetCryptoEncoder()->Debug(&Serial);
					Serial.println();
#ifdef LOLA_LINK_USE_TOKEN_HOP
					Serial.print(F("\tProtected with 32 bit TOTP @ "));
					Serial.print(LOLA_LINK_SERVICE_LINKED_TIMED_HOP_PERIOD_MILLIS);
					Serial.println(F(" ms"));
#endif
#else
					Serial.println(F("Link unsecured."));
#endif
#endif

#ifdef LOLA_LINK_ACTIVITY_LED
					pinMode(LOLA_LINK_ACTIVITY_LED, OUTPUT);
#endif
					ResetStateStartTime();
					SetNextRunASAP();

					return true;
				}
			}

		}

		return false;
	}

	void OnSendOk(const uint8_t header, const uint32_t sendDuration)
	{
		LastSentMillis = millis();

		if (header == DefinitionReport.GetHeader() &&
			LinkInfo->HasLink() &&
			ReportPending)
		{
			ReportPending = false;
			LinkInfo->StampLocalInfoLastUpdatedRemotely();
		}

		SetNextRunASAP();

#ifdef LOLA_LINK_ACTIVITY_LED
		digitalWrite(LOLA_LINK_ACTIVITY_LED, LOW);
#endif
	}

	void OnLinkInfoReportReceived(const uint8_t rssi, const uint8_t partnerReceivedCount)
	{
		if (LinkInfo->HasLink())
		{
			LinkInfo->StampPartnerInfoUpdated();
			LinkInfo->SetPartnerRSSINormalized(rssi);
			LinkInfo->SetPartnerReceivedCount(partnerReceivedCount);

			SetNextRunASAP();
		}
	}

	void SetLinkingState(const uint8_t linkingState)
	{
		LinkingState = linkingState;
		ResetLastSentTimeStamp();

		SetNextRunASAP();
	}

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
				KeyExchanger.GenerateNewKeyPair();
				KeysLastGenerated = millis();
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
			if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_LINKING_CANCEL)
			{
				UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
			}
			else
			{
				OnLinking();
			}
			break;
		case LoLaLinkInfo::LinkStateEnum::Linked:
			if (LoLaDriver->GetElapsedMillisLastValidReceived() > LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT)
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
			if (GetElapsedMillisSinceLastSent() > LOLA_LINK_SERVICE_LINKED_RESEND_PERIOD)
			{
				PrepareLinkReport(LinkInfo->GetPartnerLastReportElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_STALE_PERIOD);
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
		}
		else if (LoLaDriver->GetElapsedMillisLastValidReceived() > LOLA_LINK_SERVICE_LINKED_PERIOD_INTERVENTION)
		{
			if (!ReportPending)
			{
				ReportPending = true; //Send link info update, deferred.
				SetNextRunASAP();
			}
			else
			{
				SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD);
			}
		}
		else if (LinkInfo->GetLocalInfoUpdateRemotelyElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_UPDATE_PERIOD)
		{
			ReportPending = true; //Send link info update, deferred.
			SetNextRunDelay(random(0, LOLA_LINK_SERVICE_LINKED_UPDATE_RANDOM_JITTER_MAX));
		}
		else
		{
			OnKeepingLink();
		}

#ifdef LOLA_LINK_ACTIVITY_LED
		if (!digitalRead(LOLA_LINK_ACTIVITY_LED))
		{
			if (millis() - LastSentMillis > 50)
			{
				digitalWrite(LOLA_LINK_ACTIVITY_LED, HIGH);
			}
		}
#endif

#ifdef DEBUG_LOLA
		if (LinkInfo->HasLink())
		{
			if (LinkInfo->GetLinkDurationMillis() >= NextDebug)
			{
				DebugLinkStatistics(&Serial);

				NextDebug = (LinkInfo->GetLinkDurationMillis() / 1000) * 1000 + LOLA_LINK_DEBUG_UPDATE_MILLIS;

				if (LinkInfo->GetLinkDurationMillis() < LOLA_LINK_DEBUG_UPDATE_MILLIS)
				{
					NextDebug -= (LinkInfo->GetLinkDurationMillis() / 1000) * 1000;
				}
			}
		}
#endif
	}

protected:
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

		LoLaDriver->GetCryptoEncoder()->Clear();
		CryptoToken->SetSeed(0);

		KeyExchanger.ClearPartner();

		InfoSyncStage = 0;

		OnClearSession();

#ifdef DEBUG_LOLA
		NextDebug = LinkInfo->GetLinkDurationMillis() + 10000;
#endif
	}

	void UpdateLinkState(const LoLaLinkInfo::LinkStateEnum newState)
	{
		if (LinkInfo->GetLinkState() != newState)
		{
#ifdef DEBUG_LOLA
			if (newState == LoLaLinkInfo::LinkStateEnum::Linked)
			{
				LinkingDuration = GetElapsedMillisSinceStateStart();
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
				ChannelManager.ResetChannel();
				LoLaDriver->OnStart();
				ServicesManager->NotifyServicesLinkUpdated(false);
			}

			switch (newState)
			{
			case LoLaLinkInfo::LinkStateEnum::Setup:
				LoLaDriver->Enable();
				SetLinkingState(0);
				ClearSession();
				ChannelManager.ResetChannel();
				LoLaDriver->OnStart();
				SetNextRunASAP();
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
				ClearSession();
				SetLinkingState(0);
				PowerBalancer.SetMaxPower();
				ChannelManager.ResetChannel();
				SetNextRunASAP();
#ifdef LOLA_LINK_ACTIVITY_LED
				digitalWrite(LOLA_LINK_ACTIVITY_LED, LOW);
#endif
				break;
			case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
				ClearSession();
				SetLinkingState(0);
				ChannelManager.ResetChannel();
				LoLaDriver->OnStart();
				//Sleep time is set on Host/Remote virtual.
				break;
			case LoLaLinkInfo::LinkStateEnum::Linking:
				SetLinkingState(0);
				CryptoToken->SetSeed(LoLaDriver->GetCryptoEncoder()->GetSeed());
				if (!LoLaDriver->GetCryptoEncoder()->SetEnabled())
				{
#ifdef DEBUG_LOLA				
					Serial.println(F("Failed to start Crypto."));
#endif
					UpdateLinkState(LoLaLinkInfo::LinkStateEnum::AwaitingLink);
					break;
				}
#ifdef DEBUG_LOLA				
				Serial.print(F("Linking to Id: "));
				Serial.print(LinkInfo->GetPartnerId());
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
				LoLaDriver->ResetStatistics();
#ifdef DEBUG_LOLA
				Serial.println();
				DebugLinkEstablished();
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
				OnLinkInfoReportReceived(receivedPacket->GetPayload()[0], receivedPacket->GetPayload()[1]);
				break;

				//To Host.
			case LOLA_LINK_SUBHEADER_INFO_SYNC_HOST:
				OnHostInfoSyncReceived(receivedPacket->GetPayload()[0],
					(uint16_t)(receivedPacket->GetPayload()[1] + (receivedPacket->GetPayload()[2] << 8)));
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


protected:
	void PreparePublicKeyPacket(const uint8_t subHeader)
	{
		PrepareLongPacket(LinkInfo->GetSessionId(), subHeader);

		if (!KeyExchanger.GetPublicKeyCompressed(&OutPacket.GetPayload()[1]))
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

		OutPacket.GetPayload()[0] = LinkInfo->GetRSSINormalized();
		OutPacket.GetPayload()[1] = LoLaDriver->GetReceivedCount() % UINT8_MAX;
	}


#ifdef DEBUG_LOLA
private:
	void DebugLinkStatistics(Stream* serial)
	{
		serial->println();
		serial->println(F("Link Info"));


		serial->print(F("UpTime: "));

		uint32_t AliveSeconds = (LinkInfo->GetLinkDurationMillis() / 1000L);

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

		serial->print(F("ClockSync: "));
		serial->println(LoLaDriver->GetClockSource()->GetSyncMicros()/1000);


		serial->print(F("Transmit Power: "));
		serial->print((float)(((LinkInfo->GetTransmitPowerNormalized() * 100) / UINT8_MAX)), 0);
		serial->println(F(" %"));

		serial->print(F("RSSI: "));
		serial->print((float)(((LinkInfo->GetRSSINormalized() * 100) / UINT8_MAX)), 0);
		serial->println(F(" %"));
		serial->print(F("RSSI Partner: "));
		serial->print((float)(((LinkInfo->GetPartnerRSSINormalized() * 100) / UINT8_MAX)), 0);
		serial->println(F(" %"));

		serial->print(F("Sent: "));
		serial->println(LoLaDriver->GetTransmitedCount());
		serial->print(F("Lost: "));
		serial->println(LinkInfo->GetLostCount());
		serial->print(F("Received: "));
		serial->println(LoLaDriver->GetReceivedCount());
		serial->print(F("Rejected: "));
		serial->print(LoLaDriver->GetRejectedCount());

		serial->print(F(" ("));
		if (LoLaDriver->GetReceivedCount() > 0)
		{
			serial->print((float)(LoLaDriver->GetRejectedCount() * 100) / (float)LoLaDriver->GetReceivedCount(), 2);
			serial->println(F(" %)"));
		}
		else if (LoLaDriver->GetRejectedCount() > 0)
		{
			serial->println(F("100 %)"));
		}
		else
		{
			serial->println(F("0 %)"));
		}
		serial->print(F("Timming Collision: "));
		serial->println(LoLaDriver->GetTimingCollisionCount());

		serial->print(F("ClockSync adjustments: "));
		serial->println(LinkInfo->GetClockSyncAdjustments());
		serial->println();
	}

	void DebugLinkEstablished()
	{
		Serial.print(F("Linked: "));
		Serial.println(LoLaDriver->GetClockSource()->GetSyncMicros() / 1000);
		Serial.print(F("Linking took "));
		Serial.print(LinkingDuration);
		Serial.println(F(" ms."));
		Serial.print(F("Round Trip Time: "));
		Serial.print(LinkInfo->GetRTT());
		Serial.println(F(" us"));
		Serial.print(F("Latency compensation: "));
		Serial.print(LoLaDriver->GetETTMMicros());
		Serial.println(F(" us"));
	}
#endif

};
#endif