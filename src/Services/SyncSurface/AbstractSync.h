// AbstractSurface.h

#ifndef _ABSTRACTSURFACE_h
#define _ABSTRACTSURFACE_h

#define LOLA_SYNC_FULL_DEBUG

#include <Arduino.h>
#include <Crypto\TinyCRC.h>
#include <Services\IPacketSendService.h>
#include <Services\SyncSurface\ITrackedSurface.h>

#define ABSTRACT_SURFACE_DEFAULT_HASH 0

#define ABSTRACT_SURFACE_MAX_ELAPSED_BEFORE_SYNC_RESTART	1000
#define ABSTRACT_SURFACE_MAX_ELAPSED_BEFORE_RESYNC_DEMOTE	500
#define ABSTRACT_SURFACE_SYNC_REPLY_TIMEOUT					100
#define ABSTRACT_SURFACE_SYNC_SEND_COALESCE_PERIOD			50

#define ABSTRACT_SURFACE_SYNC_PERSISTANCE_COUNT				3
#define ABSTRACT_SURFACE_SYNC_KEEP_ALIVE_MILLIS				3000

#define ABSTRACT_SURFACE_SYNC_REPLY_CHECK_PERIOD			(ABSTRACT_SURFACE_SYNC_REPLY_TIMEOUT/(ABSTRACT_SURFACE_SYNC_PERSISTANCE_COUNT/2))
#define ABSTRACT_SURFACE_SYNC_RETRY_PERIDO					(ABSTRACT_SURFACE_SYNC_REPLY_TIMEOUT/ABSTRACT_SURFACE_SYNC_PERSISTANCE_COUNT)
#define ABSTRACT_SURFACE_SYNC_PERSISTANCE_PERIOD			(ABSTRACT_SURFACE_SYNC_KEEP_ALIVE_MILLIS/ABSTRACT_SURFACE_SYNC_PERSISTANCE_COUNT)

class AbstractSync : public IPacketSendService
{
private:
	uint8_t LastRemoteHash = ABSTRACT_SURFACE_DEFAULT_HASH;
	uint32_t LastRemoteHashReceived = ILOLA_INVALID_MILLIS;

	uint32_t StateStartTime = ILOLA_INVALID_MILLIS;
	uint32_t SubStateStart = ILOLA_INVALID_MILLIS;

protected:
	enum SyncStateEnum : uint8_t
	{
		Starting = 0,
		WaitingForTrigger = 1,
		FullSync = 2,
		Synced = 3,
		Resync = 4,
		Disabled = 5
	};

	ITrackedSurface * TrackedSurface = nullptr;

	SyncStateEnum SyncState = SyncStateEnum::Disabled;
	TemplateLoLaPacket<LOLA_PACKET_SLIM_SIZE> PacketHolder;

protected:
	virtual void OnWaitingForTriggerService() {}
	virtual void OnSyncActive() {}
	virtual void OnSyncedService() {}

	virtual void OnSurfaceDataChanged() {}
	virtual void OnStateUpdated(const SyncStateEnum newState) {}

public:
	AbstractSync(Scheduler* scheduler, const uint16_t period, ILoLa* loLa, ITrackedSurface* trackedSurface)
		: IPacketSendService(scheduler, period, loLa, &PacketHolder)
	{
		TrackedSurface = trackedSurface;
		
		SyncState = SyncStateEnum::Disabled;
	}

	ITrackedSurface* GetSurface()
	{
		return TrackedSurface;
	}

	void OnLinkEstablished()
	{
		Enable();
		SetNextRunASAP();
		UpdateState(SyncStateEnum::Starting);
	}

	void OnLinkLost()
	{
		UpdateState(SyncStateEnum::Disabled);
	}

	bool IsSynced()
	{
		return SyncState == SyncStateEnum::Synced;
	}

	bool IsSyncEnabled()
	{
		return SyncState != SyncStateEnum::Disabled;
	}

	bool IsSyncing()
	{
		return SyncState == SyncStateEnum::Resync || SyncState == SyncStateEnum::FullSync;
	}

	void SurfaceDataChangedEvent(uint8_t param)
	{
		OnSurfaceDataChanged();
	}

	void NotifyDataChanged()
	{
		if (TrackedSurface != nullptr)
		{
			TrackedSurface->NotifyDataChanged();
		}
	}

protected:
	virtual bool OnEnable()
	{
		return true;
	}

	void SetRemoteHash(const uint8_t remoteHash)
	{
		LastRemoteHashReceived = Millis();
		if (LastRemoteHashReceived == ILOLA_INVALID_MILLIS)
		{
			LastRemoteHashReceived--;
		}
		LastRemoteHash = remoteHash;
	}

	bool IsLastRemoteHashFresh()
	{
		if (LastRemoteHashReceived != ILOLA_INVALID_MILLIS)
		{
			return (Millis() - LastRemoteHashReceived < ABSTRACT_SURFACE_SYNC_PERSISTANCE_PERIOD);
		}
		return true;
	}

	bool IsLastRemoteHashStale()
	{
		if (LastRemoteHashReceived != ILOLA_INVALID_MILLIS)
		{
			return (Millis() - LastRemoteHashReceived > ABSTRACT_SURFACE_SYNC_KEEP_ALIVE_MILLIS);
		}
		return true;
	}

	inline bool HasRemoteHash()
	{
		return LastRemoteHashReceived != ILOLA_INVALID_MILLIS;
	}

	bool HashesMatch()
	{
#ifdef DEBUG_LOLA
		if (!HasRemoteHash())
		{
			Serial.println(F("No Remote hash"));
		}
		if (GetLocalHash() != LastRemoteHash)
		{
			Serial.println(F("Hash mismatch"));
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
		LastRemoteHashReceived = ILOLA_INVALID_MILLIS;
	}

	inline uint32_t GetSubStateElapsed()
	{
		if (SubStateStart != ILOLA_INVALID_MILLIS)
		{
			return Millis() - SubStateStart;
		}
		else
		{
			return 0;
		}
	}

	void StampSubStateStart(const int16_t offset = 0)
	{
		SubStateStart = Millis() + offset;
	}

	void ResetSubStateStart()
	{
		SubStateStart = ILOLA_INVALID_MILLIS;
	}

	uint32_t GetElapsedSinceStateStart()
	{
		if (StateStartTime != ILOLA_INVALID_MILLIS)
		{
			return Millis() - StateStartTime;
		}
		else
		{
			return ILOLA_INVALID_MILLIS;
		}
	}

	void UpdateState(const SyncStateEnum newState)
	{
		if (SyncState != newState)
		{
			StateStartTime = Millis();

			SetNextRunASAP();

			if (SyncState == SyncStateEnum::Disabled &&
				newState != SyncStateEnum::Disabled)
			{
				Enable();
			}

#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.print(Millis());
			Serial.print(F(": Updated State to "));
#endif
			switch (newState)
			{
			case SyncStateEnum::WaitingForTrigger:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("WaitingForTrigger"));
#endif
				break;
			case SyncStateEnum::Starting:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Starting"));
#endif
				InvalidateLocalHash();
				break;
			case SyncStateEnum::FullSync:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("FullSync"));
#endif
				InvalidateRemoteHash();
				TrackedSurface->GetTracker()->SetAll();
				Enable();
				break;
			case SyncStateEnum::Synced:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Synced"));
				TrackedSurface->GetTracker()->ClearAll();
#endif
				break;
			case SyncStateEnum::Resync:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Resync"));
#endif
				break;
			case SyncStateEnum::Disabled:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Disabled"));
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

	virtual bool OnSetup()
	{
		if (IPacketSendService::OnSetup() && TrackedSurface != nullptr)
		{
			MethodSlot<AbstractSync, uint8_t> memFunSlot(this, &AbstractSync::SurfaceDataChangedEvent);
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
		case SyncStateEnum::Starting:
			SetNextRunASAP();
			UpdateState(SyncStateEnum::WaitingForTrigger);
			break;
		case SyncStateEnum::WaitingForTrigger:
			OnWaitingForTriggerService();
			break;
		case SyncStateEnum::Resync:
		case SyncStateEnum::FullSync:
			OnSyncActive();
			break;
		case SyncStateEnum::Synced:
			OnSyncedService();
			break;
		case SyncStateEnum::Disabled:
		default:
			SyncState = SyncStateEnum::Disabled;
			Disable();
			break;
		}
	}
};
#endif