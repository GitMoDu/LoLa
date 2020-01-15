// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h

#define DEBUG_LINK_SERVICE


#include <Services\Link\AbstractLinkService.h>

#include <LoLaClock\TickTrainedTimerSyncedClock.h>

#include <Services\Link\LoLaLinkClockSyncer.h>

#include <LoLaCrypto\LoLaCryptoKeyExchanger.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>

#include <Services\Link\LoLaLinkPowerBalancer.h>
#include <Services\Link\LoLaLinkChannelManager.h>


#include <PacketDriver\LinkIndicator.h>
#include <PacketDriver\ILoLaSelector.h>


#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
#include <Services\Link\PingService.h>
#include <Services\Link\LinkDebugTask.h>
#endif


class LoLaLinkService : public AbstractLinkService
{
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
protected:
	LinkDebugTask DebugTask;
#endif

private:
	// Channel manager.
	LoLaLinkChannelManager ChannelSelector;

	// Power balancer.
	LoLaLinkPowerBalancer PowerBalancer;


	// Link report tracking.
	bool ReportPending = false;

protected:
	// Host/Remote clock syncer.
	LoLaLinkClockSyncer* ClockSyncer = nullptr;

	// Crypto encode.
	LoLaCryptoEncoder CryptoEncoder;

	// Synced clock.
	TickTrainedTimerSyncedClock SyncedClock;

	// Crypto key exchanger.
	LoLaCryptoKeyExchanger	KeyExchanger;

	// Shared Sub state helpers.
	enum LinkingStagesEnum : uint8_t
	{
		ClockSyncStage = 1,
		LinkProtocolSwitchOver = 2,
		LinkingDone = 3
	} LinkingState = LinkingStagesEnum::ClockSyncStage;

	enum ProcessingDHEnum : uint8_t
	{
		GeneratingSecretKey = 0,
		ExpandingKey = 1
	} ProcessingDHState = ProcessingDHEnum::GeneratingSecretKey;

	// Link Info.
	LoLaLinkInfo LinkInfo;

	// Random salt, unique for each session.
	uint32_t SessionSalt = 0;

	// Used to keep track of establishing links.
	uint8_t SessionId = 0;

protected:
	///Internal housekeeping.
	virtual void OnStateChanged() {}
	virtual uint32_t GetSleepDelay() { return LOLA_LINK_SERVICE_CHECK_PERIOD; }
	virtual void ResetLinkingState() {}
	virtual const uint32_t GetAwaitingSleepDuration() { return 1; }//TODO: Revise.

	// Host packet handling.
	virtual void OnPKERequestReceived(const uint8_t sessionId, const uint32_t protocolCode, uint8_t* remotePublicKey) {}
	virtual bool OnLinkSwitchOverReceived(const uint8_t sessionId) {}

	// Remote packet handling.
	virtual bool OnPKEResponseReceived(const uint8_t sessionId, const uint32_t sessionSalt, uint8_t* hostPublicKey) {}

	// Runtime handlers.
	virtual void OnLinking() { SetNextRunDelay(LOLA_LINK_SERVICE_CHECK_PERIOD); }
	virtual void OnAwaitingLink() {}
	virtual void OnKeepingLink() { SetNextRunDelay(LOLA_LINK_SERVICE_IDLE_PERIOD); }

public:
	LoLaLinkService(Scheduler* servicesScheduler, Scheduler* driverScheduler, ILoLaDriver* driver)
		: AbstractLinkService(driverScheduler, driver)
		, SyncedClock()
		, CryptoEncoder()
		, LinkInfo()
		, PowerBalancer()
		, ChannelSelector(driverScheduler)
		, KeyExchanger()
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
		, DebugTask(servicesScheduler, driver, &LinkInfo, &SyncedClock, &KeyExchanger)
#endif
	{
		driver->SetCryptoEncoder(&CryptoEncoder);
		driver->SetTransmitPowerSelector(&PowerBalancer);
		driver->SetChannelSelector(&ChannelSelector);
		driver->SetSyncedClock(&SyncedClock);
	}

	bool OnEnable()
	{
		UpdateLinkState(LinkStateEnum::StartLink);

		return true;
	}

	void OnDisable()
	{
		UpdateLinkState(LinkStateEnum::Disabled);
	}

	LoLaLinkInfo* GetLinkInfo()
	{
		return &LinkInfo;
	}

	LinkIndicator* GetLinkIndicator()
	{
		return &LinkInfo;
	}

	virtual bool Setup()
	{
		if (AbstractLinkService::Setup() &&
			ClockSyncer != nullptr &&
			LinkInfo.Setup(LoLaDriver) &&
			SyncedClock.Setup() &&
			CryptoEncoder.Setup(&SyncedClock) &&
			PowerBalancer.Setup(LoLaDriver, &LinkInfo) &&
			KeyExchanger.Setup() &&
			ChannelSelector.Setup(LoLaDriver, CryptoEncoder.GetChannelTokenSource(), &SyncedClock))
		{
			LinkInfo.Reset();
			KeyExchanger.GenerateNewKeyPair();

			ClearSession();

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
			DebugTask.Start();
#endif
			ResetStateStartTime();
			SetNextRunASAP();

			return true;

		}

		return false;
	}

	bool OnPacketReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp, uint8_t* payload)
	{
		// Switch the packet to the appropriate method.
		switch (header)
		{
		case LOLA_LINK_HEADER_PKE_REQUEST:
			// To Host. Unencrypted packets (Expanded key set to all zeros.)
			ATUI_R.array[0] = payload[0];
			ATUI_R.array[1] = payload[1];
			ATUI_R.array[2] = payload[2];
			ATUI_R.array[3] = payload[3];
			OnPKERequestReceived(id, ATUI_R.uint, &payload[sizeof(uint32_t)]);
			break;
		case LOLA_LINK_HEADER_PKE_RESPONSE:
			// To remote. Unencrypted packets (Expanded key set to all zeros.)
			ATUI_R.array[0] = payload[0];
			ATUI_R.array[1] = payload[1];
			ATUI_R.array[2] = payload[2];
			ATUI_R.array[3] = payload[3];
			return OnPKEResponseReceived(id, ATUI_R.uint, &payload[sizeof(uint32_t)]);
		case LOLA_LINK_HEADER_LINK_START:
			return OnLinkSwitchOverReceived(id);
			break;
		case LOLA_LINK_HEADER_MULTI:
			OnClockSyncReceived(id, timestamp, payload);
			break;
		case LOLA_LINK_HEADER_REPORT:
			OnInfoReportReceived(id, payload);
			return true;
		default:
			break;
		}

		return false;
	}

	void OnTransmitted(const uint8_t header, const uint8_t id, const uint32_t transmitDuration, const uint32_t sendDuration)
	{
		LastSentMillis = millis();

		SetNextRunASAP();
	}

protected:
	void SetClockSyncer(LoLaLinkClockSyncer* clockSyncer)
	{
		ClockSyncer = clockSyncer;
	}

	void OnClockSyncReceived(const uint8_t id, const uint32_t timestamp, uint8_t* payload)
	{
		if (LinkState == LinkStateEnum::Linked || LinkState == LinkStateEnum::Linking)
		{
			//ATUI_R.uint = (SyncedClock.GetSyncMicros() + LoLaDriver->GetETTMMicros()) % UINT32_MAX;
			ClockSyncer->OnReceived(id, timestamp, payload);
			SetNextRunASAP();
		}
	}

	void OnInfoReportReceived(const uint8_t id, uint8_t* payload)
	{
		if (LinkState == LinkStateEnum::Linking)
		{
			LinkInfo.StampPartnerInfoUpdated();
			LinkInfo.SetPartnerRSSINormalized(payload[0]);
			LinkInfo.PartnerReceivedCount = payload[1];

			ReportPending = id > 0;

			SetNextRunASAP();
		}
	}

	void OnService()
	{
		switch (LinkState)
		{
		case LinkStateEnum::StartLink:
			LoLaDriver->Start();
			//TODO: Replace with constant random address, bitcoin style.
			// the public key is the address.
			//TODO: Sign the address with the certificate.
			KeyExchanger.GenerateNewKeyPair();
			UpdateLinkState(LinkStateEnum::AwaitingLink);
			break;
		case LinkStateEnum::AwaitingSleeping:
			UpdateLinkState(LinkStateEnum::AwaitingLink);
			break;
		case LinkStateEnum::AwaitingLink:
			if (LOLA_LINK_SERVICE_UNLINK_KEY_PAIR_LIFETIME != 0)
			{
				if (KeyExchanger.GetKeyPairAgeMillis() > LOLA_LINK_SERVICE_UNLINK_KEY_PAIR_LIFETIME)
				{
					KeyExchanger.GenerateNewKeyPair();
					SetNextRunASAP();
					return;
				}
			}
			OnAwaitingLink();
			break;
		case LinkStateEnum::Linking:
			if (GetElapsedMillisSinceStateStart() > LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_LINKING_CANCEL)
			{
				UpdateLinkState(LinkStateEnum::AwaitingLink);
			}
			else
			{
				OnLinking();
			}
			break;
		case LinkStateEnum::Linked:
			if (LoLaDriver->GetElapsedMillisLastValidReceived() > LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT)
			{
				UpdateLinkState(LinkStateEnum::AwaitingLink);
			}
			else
			{
				OnKeepingLinkCommon();
			}
			break;
		case LinkStateEnum::Disabled:
		default:
			Disable();
			return;
		}
	}

	bool ProcessClockSync()
	{
		if (ClockSyncer->HasResponse())
		{
			OutPacket.SetDefinition(&DefinitionMulti);
			OutPacket.SetId(ClockSyncer->GetResponse(OutPacket.GetPayload()));
			RequestSendPacket();

			return true;
		}

		return false;
	}

private:
	void OnKeepingLinkCommon()
	{
		if (!ReportPending)
		{
			ReportPending = LoLaDriver->GetElapsedMillisLastValidReceived() > LOLA_LINK_SERVICE_LINKED_PERIOD_INTERVENTION;
			ReportPending |= LinkInfo.GetLocalInfoUpdateRemotelyElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_UPDATE_PERIOD;
		}

		if (PowerBalancer.Update())
		{
			SetNextRunASAP();
		}
		else if (ReportPending) // Keep link info up to date.
		{
			PrepareLinkReport(LinkInfo.GetPartnerLastReportElapsed() > LOLA_LINK_SERVICE_LINKED_INFO_STALE_PERIOD);
			ReportPending = false;
			LinkInfo.StampLocalInfoLastUpdatedRemotely();
			RequestSendPacket();
		}
		else if (ProcessClockSync())
		{
			// Sending packet.
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

		ClockSyncer->Reset();

		LinkInfo.Reset();
		CryptoEncoder.Clear();
		LoLaDriver->ResetLiveData();

		KeyExchanger.ClearPartner();

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
		DebugTask.Reset();
#endif
	}

	void UpdateLinkState(const LinkStateEnum newState)
	{
		if (LinkState != newState)
		{
			ResetStateStartTime();
			ResetLastSentTimeStamp();

			switch (newState)
			{
			case LinkStateEnum::StartLink:
				ClearSession();
				ResetLinkingState();
				SetNextRunASAP();
				break;
			case LinkStateEnum::AwaitingLink:
				ClearSession();
				ResetLinkingState();
				CryptoEncoder.Clear();
				PowerBalancer.SetMaxPower();
				LoLaDriver->OnChannelUpdated();
				SetNextRunASAP();
				break;
			case LinkStateEnum::AwaitingSleeping:
				ClearSession();
				ResetLinkingState();
				CryptoEncoder.Clear();
				SetNextRunDelay(GetSleepDelay());
				break;
			case LinkStateEnum::Linking:
				ResetLinkingState();
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
				DebugTask.OnLinkingStarted();
#endif
				SetNextRunASAP();
				break;
			case LinkStateEnum::Linked:
				CryptoEncoder.EnableTOTP();
				LinkInfo.StampLinkStarted();
				ClockSyncer->StampSynced();
				LoLaDriver->ResetStatistics();
				SetNextRunASAP();
				break;
			case LinkStateEnum::Disabled:
				LinkInfo.Reset();
				LoLaDriver->Stop();
			default:
				break;
			}

			LinkState = newState;
			OnStateChanged();

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_SERVICE)
			DebugTask.OnLinkStateChanged(LinkState);
#endif

			// If link status changed, 
			// notify all link dependent services they can start/stop.
			if (LinkInfo.UpdateState(LinkState == LinkStateEnum::Linked))
			{
				NotifyServicesLinkStatusChanged();
			}
		}
	}

protected:
	///// Linking time packets.
	void PrepareLinkReport(const bool requestReply)
	{
		OutPacket.SetDefinition(&DefinitionReport);
		OutPacket.SetId(requestReply ? UINT8_MAX : 0);

		OutPacket.GetPayload()[0] = LinkInfo.GetRSSINormalized();
		OutPacket.GetPayload()[1] = LoLaDriver->GetReceivedCount() % UINT8_MAX;
	}
};
#endif