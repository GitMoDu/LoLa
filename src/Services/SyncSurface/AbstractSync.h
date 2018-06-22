// AbstractSurface.h

#ifndef _ABSTRACTSURFACE_h
#define _ABSTRACTSURFACE_h

#include <Arduino.h>
#include <Crypto\TinyCRC.h>
#include <Services\IPacketSendService.h>


#define LOLA_SURFACE_EVENTS_PRIORITY ProcPriority::MEDIUM_PRIORITY

class AbstractSync : public IPacketSendService
{
public:
	AbstractSync(Scheduler* scheduler, const uint16_t period, ILoLa* loLa)
		: IPacketSendService(scheduler, period, loLa, &PacketHolder)
	{
		CalculatorCRC.Reset();
		SyncState = SyncStateEnum::Disabled;
	}


	uint32_t GetLastSynced() const
	{
		return LastSynced;
	}

	void InvalidateSync()
	{
		if (SyncState >= SyncStateEnum::Synced)
		{
			SyncState = SyncStateEnum::NotSynced;
			ClearSendRequest();
			OnSyncLost();
		}
		SetNextRunASAP();
	}

	void OnLinkEstablished()
	{
		SyncState = SyncStateEnum::Starting;
		Enable();
		SetNextRunASAP();
	}

	void OnLinkLost()
	{
		if (SyncState != SyncStateEnum::Disabled)
		{
			SyncState = SyncStateEnum::Disabled;
			SetNextRunASAP();
		}
	}

private:
	enum SyncStateEnum
	{
		Starting,
		NotSynced,
		Synced,
		Disabled
	} SyncState;

	boolean HashNeedsUpdate = false;

protected:
	TinyCrc CalculatorCRC;
	uint32_t LastReceived = 0;
	uint32_t LastSynced = 0;
	LoLaPacketSlim PacketHolder;

protected:
	void TimeStampReceived(const uint32_t timeStamp)
	{
		LastReceived = timeStamp;
	}

	virtual void UpdateHash()
	{
		HashNeedsUpdate = false;
		CalculatorCRC.Reset();
	}

	uint8_t GetHash()
	{
		return CalculatorCRC.GetCurrent();
	}

	void InvalidateHash()
	{
		HashNeedsUpdate = true;
	}

protected:
	virtual bool OnSyncStart() { return true; }
	virtual bool OnNotSynced() { return true; }
	virtual bool OnSynced() { return true; }
	virtual void OnSyncAchieved() {}
	virtual void OnSyncLost() {}

public:
	virtual void NotifyDataChanged() {}

protected:
	void PromoteToSynced()
	{
		LastSynced = Millis();
		SyncState = SyncStateEnum::Synced;
		OnSyncAchieved();
		SetNextRunASAP();
	}

	bool IsSynced()
	{
		return SyncState == SyncStateEnum::Synced;
	}

	virtual bool OnSetup()
	{
		if (IPacketSendService::OnSetup())
		{
			return true;
		}

		return false;
	}

	virtual bool OnEnable()
	{
		HashNeedsUpdate = true;
		return true;
	}

	void OnService()
	{
		//Make sure hash is up to date.
		if (HashNeedsUpdate)
		{
			UpdateHash();
		}

		switch (SyncState)
		{
		case SyncStateEnum::Starting:
			SetNextRunASAP();
			if (OnSyncStart())
			{
				SyncState = SyncStateEnum::NotSynced;
			}
			else
			{
				SyncState = SyncStateEnum::Disabled;
			}
			break;
		case SyncStateEnum::NotSynced:
			if (OnNotSynced())
			{
				PromoteToSynced();
			}
			break;
		case SyncStateEnum::Synced:
			if (OnSynced())
			{
				InvalidateSync();
			}
			break;
		case SyncStateEnum::Disabled:
			SetNextRunLong();
			break;
		default:
			SyncState = SyncStateEnum::Disabled;
			break;
		}
	}	
};
#endif

