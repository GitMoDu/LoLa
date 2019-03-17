// AbstractSurface.h

#ifndef _ABSTRACTSURFACE_h
#define _ABSTRACTSURFACE_h

//#define LOLA_SYNC_FULL_DEBUG

#include <Arduino.h>
#include <Services\IPacketSendService.h>
#include <Services\SyncSurface\ITrackedSurface.h>

#define ABSTRACT_SURFACE_DEFAULT_HASH 0

#define ABSTRACT_SURFACE_MAX_ELAPSED_NO_DISCOVERY_MILLIS	(uint32_t)500

#define ABSTRACT_SURFACE_FAST_CHECK_PERIOD_MILLIS			(uint32_t)1
#define ABSTRACT_SURFACE_SLOW_CHECK_PERIOD_MILLIS			(uint32_t)200

#define ABSTRACT_SURFACE_SERVICE_DISCOVERY_SEND_PERIOD		(uint32_t)50
#define ABSTRACT_SURFACE_UPDATE_BACK_OFF_PERIOD_MILLIS		(uint32_t)0
#define ABSTRACT_SURFACE_SYNC_CONFIRM_SEND_PERIOD_MILLIS	(uint32_t)20
#define ABSTRACT_SURFACE_SEND_FAILED_RETRY_PERIDO			(uint32_t)100


class AbstractSync : public IPacketSendService
{
private:
	uint8_t LastRemoteHash = ABSTRACT_SURFACE_DEFAULT_HASH;
	bool RemoteHashIsSet = false;

	uint32_t StateStartTime = ILOLA_INVALID_MILLIS;

	uint32_t LastSent = ILOLA_INVALID_MILLIS;

protected:
	enum SyncStateEnum : uint8_t
	{
		WaitingForServiceDiscovery = 0,
		Syncing = 1,
		Synced = 2,
		Disabled = 3
	} SyncState = SyncStateEnum::Disabled;

	ITrackedSurface * TrackedSurface = nullptr;

protected:
	virtual void OnWaitingForServiceDiscovery() {}
	virtual void OnSyncActive() {}

	virtual void OnSurfaceDataChanged() {}
	virtual void OnStateUpdated(const AbstractSync::SyncStateEnum newState) {}

public:
	AbstractSync(Scheduler* scheduler, const uint16_t period, ILoLaDriver* driver, ITrackedSurface* trackedSurface, ILoLaPacket* packetHolder)
		: IPacketSendService(scheduler, period, driver, packetHolder)
	{
		TrackedSurface = trackedSurface;

		SyncState = SyncStateEnum::Disabled;
	}

	bool OnEnable()
	{
		return true;
	}

	ITrackedSurface* GetSurface()
	{
		return TrackedSurface;
	}

	void OnLinkEstablished()
	{
		UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
	}

	void OnLinkLost()
	{
		UpdateSyncState(SyncStateEnum::Disabled);
	}

	bool IsSynced()
	{
		return SyncState == SyncStateEnum::Synced;
	}

	void SurfaceDataChangedEvent(const bool dataGood)
	{
		InvalidateLocalHash();

		if (SyncState == SyncStateEnum::Synced)
		{
			UpdateSyncState(SyncStateEnum::Syncing);
		}
	}

	void NotifyDataChanged()
	{
		if (TrackedSurface != nullptr)
		{
			TrackedSurface->NotifyDataChanged();
		}
	}

protected:
	void OnSendFailed()
	{
		//In case the send fails, this prevents from immediate resending.
		SetNextRunDelay(ABSTRACT_SURFACE_SEND_FAILED_RETRY_PERIDO);
	}

	void SetRemoteHash(const uint8_t remoteHash)
	{
		RemoteHashIsSet = true;
		LastRemoteHash = remoteHash;
	}

	inline bool HasRemoteHash()
	{
		return RemoteHashIsSet;
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
		else 
		{
			Serial.println(F("Hash match"));
		}
#endif
		return (HasRemoteHash() &&
			GetLocalHash() == LastRemoteHash);
	}

	inline void UpdateLocalHash()
	{
		TrackedSurface->UpdateHash();
	}

	inline void InvalidateLocalHash()
	{
		TrackedSurface->InvalidateHash();
	}

	inline uint8_t GetLocalHash()
	{
		return TrackedSurface->GetHash();
	}

	inline void InvalidateRemoteHash()
	{
		LastRemoteHash = ABSTRACT_SURFACE_DEFAULT_HASH;
		RemoteHashIsSet = false;
	}

	uint32_t GetElapsedSinceStateStart()
	{
		if (StateStartTime != ILOLA_INVALID_MILLIS)
		{
			return millis() - StateStartTime;
		}
		else
		{
			return ILOLA_INVALID_MILLIS;
		}
	}

	void ResetLastSentTimeStamp()
	{
		LastSent = ILOLA_INVALID_MILLIS;
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

	void UpdateSyncState(const SyncStateEnum newState)
	{
		if (SyncState != newState)
		{
			StateStartTime = millis();
			ResetLastSentTimeStamp();
			SetNextRunASAP();

			Enable(); //Make sure we are running.

#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.print(GetLoLa()->GetMillisSync());
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
				Serial.println(F("FullSync"));
#endif
				InvalidateLocalHash();
				break;
			case SyncStateEnum::Synced:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Synced"));
#endif
				InvalidateLocalHash();
				break;
			case SyncStateEnum::Disabled:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Disabled"));
#endif
				break;
			default:
				break;
			}

			SyncState = newState;

			OnStateUpdated(SyncState);
		}
	}

	virtual bool OnSetup()
	{
		if (IPacketSendService::OnSetup() && TrackedSurface != nullptr)
		{
			MethodSlot<AbstractSync, const bool> memFunSlot(this, &AbstractSync::SurfaceDataChangedEvent);
			TrackedSurface->AttachOnSurfaceUpdated(memFunSlot);
			return true;
		}

		return false;
	}

	void OnService()
	{
		//Make sure hash is up to date.
		UpdateLocalHash();

		switch (SyncState)
		{
		case SyncStateEnum::WaitingForServiceDiscovery:
			OnWaitingForServiceDiscovery();
			break;
		case SyncStateEnum::Syncing:
			OnSyncActive();
			break;
		case SyncStateEnum::Synced:
			SetNextRunLong();
			break;
		case SyncStateEnum::Disabled:
		default:
			Disable();
			break;
		}
	}
};
#endif