// AbstractSurface.h

#ifndef _ABSTRACTSURFACE_h
#define _ABSTRACTSURFACE_h

#define LOLA_SYNC_FULL_DEBUG

#include <Arduino.h>
#include <Crypto\TinyCRC.h>
#include <Services\IPacketSendService.h>
#include <Services\SyncSurface\ITrackedSurface.h>

#define ABSTRACT_SURFACE_DEFAULT_HASH 0

#define ABSTRACT_SURFACE_MAX_ELAPSED_DATA_SYNC_LOST			(uint32_t)1000

#define ABSTRACT_SURFACE_SYNC_REPLY_CHECK_PERIOD			(uint32_t)50
#define ABSTRACT_SURFACE_SYNC_RETRY_PERIDO					(ABSTRACT_SURFACE_SYNC_REPLY_CHECK_PERIOD*2)

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
	virtual void OnStateUpdated(const SyncStateEnum newState) {}

public:
	AbstractSync(Scheduler* scheduler, const uint16_t period, ILoLa* loLa, ITrackedSurface* trackedSurface, ILoLaPacket* packetHolder)
		: IPacketSendService(scheduler, period, loLa, packetHolder)
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

	void SurfaceDataChangedEvent(uint8_t param)
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
		SetNextRunDelay(ABSTRACT_SURFACE_SYNC_RETRY_PERIDO);
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
		RemoteHashIsSet = false;
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
			return Millis() - LastSent;
		}
	}

	void UpdateSyncState(const SyncStateEnum newState)
	{
		if (SyncState != newState)
		{
			StateStartTime = Millis();
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
				InvalidateRemoteHash();
				TrackedSurface->GetTracker()->SetAll();
				break;
			case SyncStateEnum::Synced:
				if (TrackedSurface->GetTracker()->HasSet())
				{
					//State progression denied while there are set bits in tracker.
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
					Serial.println(F("State progression denied while there are set bits in tracker."));
#endif
					return;
				}
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Synced"));
#endif
				InvalidateRemoteHash();
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
		case SyncStateEnum::WaitingForServiceDiscovery:
			OnWaitingForServiceDiscovery();
			break;
		case SyncStateEnum::Syncing:
			OnSyncActive();
			break;
		case SyncStateEnum::Synced:
			if (TrackedSurface->GetTracker()->HasSet() ||
				!HashesMatch())
			{
				UpdateSyncState(SyncStateEnum::Syncing);
			}
			else
			{
				SetNextRunLong();
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