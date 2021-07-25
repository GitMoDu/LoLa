// AbstractSurface.h

#ifndef _ABSTRACTSURFACE_h
#define _ABSTRACTSURFACE_h


#include <Services\LoLaPacketService.h>
#include <Callback.h>

#include <Services\SyncSurface\SyncPacketDefinitions.h>


#include <Services\SyncSurface\ITrackedSurface.h>



class AbstractSync : public LoLaPacketService<SYNC_SERVICE_PACKET_MAX_PAYLOAD_SIZE, true>
{
protected:
	static const uint32_t MAX_ELAPSED_NO_DISCOVERY_MILLIS = 1000;

	static const uint32_t FAST_CHECK_PERIOD_MILLIS = 1;

	static const uint32_t SLOW_CHECK_PERIOD_MILLIS = 100;

	static const uint32_t DISCOVERY_SEND_PERIOD = 50;
	static const uint32_t UPDATE_BACK_OFF_PERIOD_MILLIS = 5;
	static const uint32_t SYNC_CONFIRM_RESEND_PERIOD_MILLIS = 30;
	static const uint32_t SEND_FAILED_RETRY_PERIOD = 100;
	static const uint32_t RECEIVE_FAILED_PERIOD = SEND_FAILED_RETRY_PERIOD * 4;

private:
	uint8_t LastRemoteHash = 0;
	bool RemoteHashIsSet = false;

	//CRC Calculator.
	FastCRC8 CRC8;

	uint8_t LastLocalHash = 0;
	bool LocalHashIsUpToDate = false;

	uint32_t StateStartTime = 0;

protected:
	enum SyncStateEnum
	{
		Disabled = 0,
		WaitingForServiceDiscovery = 1,
		Syncing = 2,
		Synced = 3
	} SyncState = SyncStateEnum::Disabled;

	ITrackedSurface* TrackedSurface = nullptr;

	uint32_t LastSyncMillis = 0;

protected:
	virtual void OnWaitingForServiceDiscovery() {}
	virtual uint8_t GetSurfaceId() { return 0; }
	virtual void OnSyncActive() {}

	virtual void OnSurfaceDataChanged() {}
	virtual void OnStateUpdated(const SyncStateEnum newState) {}

public:
	AbstractSync(Scheduler* scheduler, const uint16_t period, LoLaPacketDriver* driver,
		ITrackedSurface* trackedSurface)
		: LoLaPacketService(scheduler, period, driver)
		, TrackedSurface(trackedSurface)
	{
		SyncState = SyncStateEnum::Disabled;
	}

	virtual bool Setup()
	{
		if (LoLaPacketService::Setup())
		{
			MethodSlot<AbstractSync, const bool> memFunSlot(this, &AbstractSync::SurfaceDataChangedEvent);
			TrackedSurface->AttachOnSurfaceUpdated(memFunSlot);

			return true;
		}

		return false;
	}

	//void SyncOverrideEnable(const bool enable)
	//{
	//	if (enable)
	//	{
	//		if (SyncState == SyncStateEnum::Disabled)
	//		{
	//			UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
	//		}
	//	}
	//	else
	//	{
	//		if (SyncState != SyncStateEnum::Disabled)
	//		{
	//			UpdateSyncState(SyncStateEnum::Disabled);
	//		}
	//	}
	//}

	virtual void OnLinkStatusChanged(const bool linked)
	{
		if (linked)
		{
			UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
		}
		else
		{
			UpdateSyncState(SyncStateEnum::Disabled);
		}
	}

	bool IsSynced()
	{
		return SyncState == SyncStateEnum::Synced;
	}

	void SurfaceDataChangedEvent(const bool ignore)
	{
		InvalidateLocalHash();

		if (SyncState == SyncStateEnum::Synced)
		{
			UpdateSyncState(SyncStateEnum::Syncing);
		}
	}

protected:
	void OnSendFailed()
	{
		//In case the send fails, this prevents from immediate resending.
		SetNextRunDelay(SEND_FAILED_RETRY_PERIOD);
	}

	uint8_t GetLocalHash()
	{
		if (!LocalHashIsUpToDate)
		{
			LocalHashIsUpToDate = true;
			LastLocalHash = CRC8.smbus(TrackedSurface->SurfaceData, TrackedSurface->DataSize);
		}

		return LastLocalHash;
	}

	void InvalidateLocalHash()
	{
		LocalHashIsUpToDate = false;
	}

	void ForceUpdateLocalHash()
	{
		LocalHashIsUpToDate = false;
		GetLocalHash();
	}

	bool HasRemoteHash()
	{
		return RemoteHashIsSet;
	}

	void SetRemoteHash(const uint8_t remoteHash)
	{
		RemoteHashIsSet = true;
		LastRemoteHash = remoteHash;
	}

	void InvalidateRemoteHash()
	{
		RemoteHashIsSet = false;
	}

	bool HashesMatch()
	{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
		if (!HasRemoteHash())
		{
			Serial.println(F("No Remote hash"));
		}
		else if (GetLocalHash() != LastRemoteHash)
		{
			Serial.println(F("Hash mismatch"));
		}
#endif
		return (HasRemoteHash() && GetLocalHash() == LastRemoteHash);
	}

	uint32_t GetElapsedSinceStateStart()
	{
		if (StateStartTime != 0)
		{
			return millis() - StateStartTime;
		}
		else
		{
			return UINT32_MAX;
		}
	}

	void UpdateSyncState(const SyncStateEnum newState)
	{
		if (SyncState != newState)
		{
			StateStartTime = millis();
			ResetLastSentTimeStamp();

			Enable(); // Enable run by default.
			SetNextRunASAP();

#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.print(LoLaDriver->GetMicros());
			Serial.print(F(": Updated State to "));
#endif

			switch (newState)
			{
			case SyncStateEnum::WaitingForServiceDiscovery:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("WaitingForServiceDiscovery"));
#endif
				break;
			case SyncStateEnum::Syncing:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Syncing"));
#endif
				if (SyncState != SyncStateEnum::Synced)
				{
					LastSyncMillis = 0;
					InvalidateLocalHash();
				}
				break;
			case SyncStateEnum::Synced:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Synced"));
#endif
				LastSyncMillis = StateStartTime;
				break;
			case SyncStateEnum::Disabled:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Sleeping"));
#endif
				Disable();
				break;
			default:
				break;
			}

			SyncState = newState;

			OnStateUpdated(SyncState);
		}
	}


	void OnService()
	{
		switch (SyncState)
		{
		case SyncStateEnum::WaitingForServiceDiscovery:
			OnWaitingForServiceDiscovery();
			break;
		case SyncStateEnum::Syncing:
			OnSyncActive();
			break;
		case SyncStateEnum::Synced:
			if (TrackedSurface->HasAnyPending())
			{
				UpdateSyncState(SyncStateEnum::Syncing);
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Abstract Sync Recovery!"));
#endif
			}
			else
			{
				SetNextRunDelay(SLOW_CHECK_PERIOD_MILLIS);
			}
			break;
		case SyncStateEnum::Disabled:
		default:
			Disable();
			break;
		}
	}
};
#endif