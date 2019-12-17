// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h

#define DEBUG_LINK_SERVICE


#include <Services\Link\AbstractLinkService.h>


#include <Services\Link\LoLaLinkClockSyncer.h>

#include <LoLaCrypto\LoLaCryptoKeyExchange.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>

#include <Services\Link\LoLaLinkPowerBalancer.h>
//#include <Services\Link\LoLaLinkChannelManager.h>

#include <Services\Link\LoLaLinkTimedHopper.h>

#include <LinkIndicator.h>

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
#include <Services\Link\PingService.h>
#include <Services\Link\LinkDebugTask.h>
#endif


class LoLaLinkService : public AbstractLinkService
{
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
protected:
	PingService LatencyService;
	LinkDebugTask DebugTask;
#endif

private:
	//Crypto Token.
	LoLaTimedCryptoTokenSource	CryptoSeed;

	//Sub-services.
	LoLaLinkTimedHopper<LoLaTimedCryptoTokenSource::PeriodMillis> TimedHopper;

	//Power balancer.
	LoLaLinkPowerBalancer PowerBalancer;

	//Channel manager.
	IChannelSelector ChannelSelector;

	//Link report tracking.
	bool ReportPending = false;

protected:
	///Synced clock
#ifdef LOLA_LINK_USE_RTC_CLOCK_SOURCE
	RTCClockSource SyncedClock;
#else
	ILoLaClockSource SyncedClock;
#endif
	///

	//Crypto key exchanger.
	LoLaCryptoKeyExchanger	KeyExchanger;
	//

	//Synced Clock.
	LoLaLinkClockSyncer* ClockSyncerPointer = nullptr;

	//Shared Sub state helpers.
	uint32_t SubStateStart = 0;
	uint8_t LinkingState = 0;
	uint8_t InfoSyncStage = 0;

	///Link Info Source
	LoLaLinkInfo LinkInfo;
	///

protected:
	///Internal housekeeping.
	virtual void OnClearSession() {}
	virtual void OnLinkStateChanged(const LinkStatus::StateEnum newState) {}

	///Host packet handling.
	//Unlinked packets.
	virtual void OnLinkDiscoveryReceived() {}
	virtual void OnPKCRequestReceived(const uint8_t sessionId, const uint32_t remoteMACHash) {}
	virtual void OnRemotePublicKeyReceived(const uint8_t sessionId, uint8_t* encodedPublicKey) {}

	//Linked packets.
	virtual void OnRemoteInfoSyncReceived(const uint8_t rssi) {}
	virtual void OnHostInfoSyncRequestReceived() {}
	virtual void OnClockSyncRequestReceived(const uint8_t requestId, const uint32_t estimatedMicros) {}
	virtual void OnClockUTCRequestReceived(const uint8_t requestId) {}
	virtual void OnClockSyncTuneRequestReceived(const uint8_t requestId, const uint32_t estimatedMicros) {}
	///

	///Remote packet handling.
	//Unlinked packets.
	virtual void OnIdBroadcastReceived(const uint8_t sessionId, const uint32_t hostMACHash) {}
	virtual void OnHostPublicKeyReceived(const uint8_t sessionId, uint8_t* hostPublicKey) {}

	//Linked packets.
	virtual void OnHostInfoSyncReceived(const uint8_t rssi, const uint16_t rtt) {}
	virtual void OnClockSyncResponseReceived(const uint8_t requestId, const int32_t estimatedErrorMicros) {}
	virtual void OnClockUTCResponseReceived(const uint8_t requestId, const uint32_t secondsUTC) {}
	virtual void OnClockSyncTuneResponseReceived(const uint8_t requestId, const int32_t estimatedErrorMicros) {}
	///

	//Runtime handlers.
	virtual void OnLinking() { SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD); }
	virtual bool OnAwaitingLink() { return false; }
	virtual void OnKeepingLink() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }


public:
	LoLaLinkService(Scheduler* servicesScheduler, Scheduler* driverScheduler, ILoLaDriver* driver)
		: AbstractLinkService(driverScheduler, driver)
		, SyncedClock()
		, LinkInfo(driver, &SyncedClock)
		, CryptoSeed(&SyncedClock)
		, TimedHopper(driverScheduler, driver, &SyncedClock)
		, PowerBalancer(driver, &LinkInfo)
		, ChannelSelector()
		, KeyExchanger()
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
		, LatencyService(servicesScheduler, driver, &LinkInfo)
		, DebugTask(servicesScheduler, driver, &LinkInfo, &SyncedClock, &KeyExchanger)
#endif
	{
		LoLaDriver->SetTransmitPowerSelector(&PowerBalancer);
		LoLaDriver->SetChannelSelector(&ChannelSelector);
		LoLaDriver->SetClockSource(&SyncedClock);

#ifdef LOLA_LINK_USE_TOKEN_HOP
		//TODO:
		LoLaDriver->SetDynamicTokenSource();
#endif

#ifdef LOLA_LINK_USE_FREQUENCY_HOP
		//TODO:
		LoLaDriver->SetDynamicChannelSource();
#endif
	}

	bool OnEnable()
	{
		UpdateLinkState(LinkStatus::StateEnum::Setup);

		return true;
	}

	void OnDisable()
	{
		UpdateLinkState(LinkStatus::StateEnum::Disabled);
	}

	LoLaLinkInfo* GetLinkInfo()
	{
		return &LinkInfo;
	}

	ILinkIndicator* GetLinkIndicator()
	{
		return &LinkInfo;
	}

	LinkStatus* GetLinkStatus()
	{
		return &LinkInfo;
	}

public:
	bool Setup()
	{
		if (AbstractLinkService::Setup() &&
#ifdef LOLA_LINK_USE_FREQUENCY_HOP
			TimedHopper.Setup() &&
#endif			
			ClockSyncerPointer != nullptr &&
			PowerBalancer.Setup() &&
			CryptoSeed.Setup() &&
			KeyExchanger.Setup() &&
			TimedHopper.Setup() &&
			ClockSyncerPointer->Setup(&SyncedClock))
		{
			SyncedClock.Start();

			LinkInfo.Reset();

			KeyExchanger.GenerateNewKeyPair();

			//Make sure to lazy load the local MAC on startup.
			LinkInfo.GetLocalId();

			ClearSession();

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
			DebugTask.Debug();
#endif
			ResetStateStartTime();
			SetNextRunASAP();

			return true;

		}

		return false;
	}

	bool OnPacketReceived(PacketDefinition* definition, const uint8_t id, uint8_t* payload, const uint32_t timestamp)
	{

		// Switch the packet to the appropriate method.
		switch (definition->GetHeader())
		{
		case LOLA_LINK_HEADER_SHORT:
			switch (payload[0])
			{
				///Unlinked packets.
				//To Host.
			case LOLA_LINK_SUBHEADER_LINK_DISCOVERY:
				OnLinkDiscoveryReceived();
				break;

			case LOLA_LINK_SUBHEADER_REMOTE_PKC_START_REQUEST:
				ArrayToR_Array(&payload[1]);
				OnPKCRequestReceived(id, ATUI_R.uint);
				break;

				//To remote.
			case LOLA_LINK_SUBHEADER_HOST_ID_BROADCAST:
				ArrayToR_Array(&payload[1]);
				OnIdBroadcastReceived(id, ATUI_R.uint);
				break;
				///

				///Linking Packets
				//To Host
			case LOLA_LINK_SUBHEADER_NTP_REQUEST:
				ArrayToR_Array(&payload[1]);
				OnClockSyncRequestReceived(id, ATUI_R.uint);
				break;
			case LOLA_LINK_SUBHEADER_UTC_REQUEST:
				OnClockUTCRequestReceived(id);
				break;

				//To Remote.
			case LOLA_LINK_SUBHEADER_NTP_REPLY:
				ArrayToR_Array(&payload[1]);
				OnClockSyncResponseReceived(id, ATUI_R.iint);
				break;

			case LOLA_LINK_SUBHEADER_UTC_REPLY:
				ArrayToR_Array(&payload[1]);
				OnClockUTCResponseReceived(id, ATUI_R.uint);
				break;
				///

				///Linked packets.
				//Host.
			case LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST:
				ArrayToR_Array(&payload[1]);
				OnClockSyncTuneRequestReceived(id, ATUI_R.uint);
				break;

				//Remote.
			case LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY:
				ArrayToR_Array(&payload[1]);
				OnClockSyncTuneResponseReceived(id, ATUI_R.iint);
				break;
				///
			default:
				break;
			}
			return true;
		case LOLA_LINK_HEADER_LONG:
			switch (payload[0])
			{
				//To Host.
			case LOLA_LINK_SUBHEADER_REMOTE_PUBLIC_KEY:
				OnRemotePublicKeyReceived(id, &payload[1]);
				break;

				//To remote.
			case LOLA_LINK_SUBHEADER_HOST_PUBLIC_KEY:
				OnHostPublicKeyReceived(id, &payload[1]);
				break;
			}
			return true;
		case LOLA_LINK_HEADER_REPORT:
			switch (id)
			{
				//To both.
			case LOLA_LINK_SUBHEADER_LINK_REPORT_WITH_REPLY:
				ReportPending = true;
			case LOLA_LINK_SUBHEADER_LINK_REPORT:
				OnLinkInfoReportReceived(payload[0], payload[1]);
				break;

				//To Host.
			case LOLA_LINK_SUBHEADER_INFO_SYNC_HOST:
				OnHostInfoSyncReceived(payload[0],
					(uint16_t)(payload[1] + (payload[2] << 8)));
				break;

				//To Remote.
			case LOLA_LINK_SUBHEADER_INFO_SYNC_REMOTE:
				OnRemoteInfoSyncReceived(payload[0]);
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

	void OnTransmitted(const uint8_t header, const uint8_t id, const uint32_t transmitDuration, const uint32_t sendDuration)
	{
		LastSentMillis = millis();

		if (header == DefinitionReport.GetHeader() &&
			LinkInfo.HasLink() &&
			ReportPending)
		{
			ReportPending = false;
			LinkInfo.StampLocalInfoLastUpdatedRemotely();
		}

		SetNextRunASAP();
	}

protected:
	void OnLinkInfoReportReceived(const uint8_t rssi, const uint8_t partnerReceivedCount)
	{
		if (LinkInfo.HasLink())
		{
			LinkInfo.StampPartnerInfoUpdated();
			LinkInfo.SetPartnerRSSINormalized(rssi);
			LinkInfo.SetPartnerReceivedCount(partnerReceivedCount);

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
		switch (LinkInfo.GetLinkState())
		{
		case LinkStatus::StateEnum::Setup:
		case LinkStatus::StateEnum::AwaitingSleeping:
			UpdateLinkState(LinkStatus::StateEnum::AwaitingLink);
			break;
		case LinkStatus::StateEnum::AwaitingLink:
			if (LOLA_LINK_SERVICE_UNLINK_KEY_PAIR_LIFETIME != 0)
			{
				if (KeyExchanger.GetKeyPairAgeMillis() > LOLA_LINK_SERVICE_UNLINK_KEY_PAIR_LIFETIME)
				{
					KeyExchanger.GenerateNewKeyPair();
					SetNextRunASAP();
					return;
				}
			}

			if (!OnAwaitingLink())//Time out is different for host/remote.
			{
				UpdateLinkState(LinkStatus::StateEnum::AwaitingSleeping);
			}
			break;
		case LinkStatus::StateEnum::Linking:
			if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_LINKING_CANCEL)
			{
				UpdateLinkState(LinkStatus::StateEnum::AwaitingLink);
			}
			else
			{
				OnLinking();
			}
			break;
		case LinkStatus::StateEnum::Linked:
			if (LoLaDriver->GetElapsedMillisLastValidReceived() > LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT)
			{
				UpdateLinkState(LinkStatus::StateEnum::AwaitingLink);
			}
			else
			{
				OnKeepingLinkCommon();
			}
			break;
		case LinkStatus::StateEnum::Disabled:
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
				PrepareLinkReport(LinkInfo.GetPartnerLastReportElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_STALE_PERIOD);
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
		else if (LinkInfo.GetLocalInfoUpdateRemotelyElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_UPDATE_PERIOD)
		{
			ReportPending = true; //Send link info update, deferred.
			SetNextRunDelay(random(0, LOLA_LINK_SERVICE_LINKED_UPDATE_RANDOM_JITTER_MAX));
		}
		else
		{
			OnKeepingLink();
		}
	}

protected:
	void ClearSession()
	{
		ReportPending = false;

		if (ClockSyncerPointer != nullptr)
		{
			ClockSyncerPointer->Reset();
		}

		LinkInfo.Reset();
		LoLaDriver->GetCryptoEncoder()->Clear();
		CryptoSeed.SetSeed(0);
		LoLaDriver->ResetLiveData();

		KeyExchanger.ClearPartner();

		InfoSyncStage = 0;
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
		LatencyService.Reset();
#endif
		OnClearSession();
	}

	void UpdateLinkState(const LinkStatus::StateEnum newState)
	{
		if (LinkInfo.GetLinkState() != newState)
		{
			ResetStateStartTime();
			ResetLastSentTimeStamp();

			bool NotifyUnlink = LinkInfo.GetLinkState() == LinkStatus::StateEnum::Linked;

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
			if (NotifyUnlink)
			{
				DebugTask.OnLinkOff();
			}
#endif

			switch (newState)
			{
			case LinkStatus::StateEnum::Setup:
				LoLaDriver->Start();
				SetLinkingState(0);
				ClearSession();
				SetNextRunASAP();
				break;
			case LinkStatus::StateEnum::AwaitingLink:
				ClearSession();
				SetLinkingState(0);
				PowerBalancer.SetMaxPower();
				LoLaDriver->OnChannelUpdated();
				SetNextRunASAP();
				break;
			case LinkStatus::StateEnum::AwaitingSleeping:
				ClearSession();
				SetLinkingState(0);
				// Sleep time is set on Host/Remote virtual.
				break;
			case LinkStatus::StateEnum::Linking:
				SetLinkingState(0);
				CryptoSeed.SetSeed(LoLaDriver->GetCryptoEncoder()->GetSeed());
				if (!LoLaDriver->GetCryptoEncoder()->SetEnabled())
				{
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
					Serial.println(F("Failed to start Crypto."));
#endif
					UpdateLinkState(LinkStatus::StateEnum::AwaitingLink);
				}
				else
				{
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
					DebugTask.OnLinkingStarted();
#endif
					SetNextRunASAP();
				}
				break;
			case LinkStatus::StateEnum::Linked:
				LinkInfo.StampLinkStarted();
				LoLaDriver->GetCryptoEncoder()->SetToken(CryptoSeed.GetSeed());
				ClockSyncerPointer->StampSynced();
				LoLaDriver->ResetStatistics();
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
				DebugTask.OnLinkOn();
#endif

				// Notify all link dependent services they can start.
				NotifyServicesLinkStatusChanged();
				SetNextRunASAP();
				break;
			case LinkStatus::StateEnum::Disabled:
				LinkInfo.Reset();
				LoLaDriver->Stop();
			default:
				break;
			}

			OnLinkStateChanged(newState);			
			LinkInfo.UpdateState(newState);

			// Previous state was linked, and now it's not.
			if (NotifyUnlink)
			{
				// Notify all link dependent services to stop.
				NotifyServicesLinkStatusChanged();
			}
		}
	}

protected:
	void PreparePublicKeyPacket(const uint8_t subHeader)
	{
		PrepareLongPacket(LinkInfo.GetSessionId(), subHeader);

		if (!KeyExchanger.GetPublicKeyCompressed(&OutPacket.GetPayload()[1]))
		{
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
			Serial.println(F("Unable to read PK"));
#endif
		}
	}

	///// Linking time packets.
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

		OutPacket.GetPayload()[0] = LinkInfo.GetRSSINormalized();
		OutPacket.GetPayload()[1] = LoLaDriver->GetReceivedCount() % UINT8_MAX;
	}

};


#endif