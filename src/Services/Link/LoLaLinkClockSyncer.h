// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <LoLaClock\ILoLaClockSource.h>
#include <Services\Link\ClockSyncTransaction.h>
#include <Services\Link\LoLaLinkDefinitions.h>

class LoLaLinkClockSyncer
{
private:
	ILoLaClockSource * SyncedClock = nullptr;

protected:
	uint8_t SyncGoodCount = 0;
	uint32_t LastSynced = ILOLA_INVALID_MILLIS;

	static const int32_t MAX_TUNE_ERROR_MICROS = 100;

protected:
	virtual void OnReset() {}

protected:
	void StampSyncGood()
	{
		if (SyncGoodCount < UINT8_MAX)
		{
			SyncGoodCount++;
		}
	}

	inline void AddOffsetMicros(const int32_t offset)
	{
		if (SyncedClock != nullptr)
		{
			SyncedClock->AddOffsetMicros(offset);
		}
	}

	void SetRandom()
	{
		if (SyncedClock != nullptr)
		{
			SyncedClock->SetRandom();
		}
	}

public:
	LoLaLinkClockSyncer() {}

	bool Setup(ILoLaClockSource * syncedClock)
	{
		SyncedClock = syncedClock;

		return SyncedClock != nullptr;
	}

	void StampSynced()
	{
		LastSynced = millis();
	}

	void Reset()
	{
		SyncGoodCount = 0;
		LastSynced = ILOLA_INVALID_MILLIS;
		OnReset();
	}

public:
	virtual bool IsSynced() { return false; }
};

class LinkRemoteClockSyncer : public LoLaLinkClockSyncer
{
private:
	boolean HostSynced = false;

protected:
	void OnReset()
	{
		HostSynced = false;
	}

public:
	LinkRemoteClockSyncer() : LoLaLinkClockSyncer()	{}

	bool IsSynced()
	{
		return HasEstimation() && HostSynced;
	}

	bool IsTimeToTune()
	{
		return LastSynced == ILOLA_INVALID_MILLIS || ((millis() - LastSynced) > (LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD));
	}

	void SetReadyForEstimation()
	{
		if (IsSynced())
		{
			LastSynced = millis() - LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD;
		}
	}

	void SetSynced()
	{
		HostSynced = true;
		StampSynced();
	}

	bool HasEstimation()
	{
		return SyncGoodCount > 0;
	}

	void OnEstimationErrorReceived(const int32_t estimationErrorMicros)
	{
		if (abs(estimationErrorMicros) < MAX_TUNE_ERROR_MICROS)
		{
			StampSyncGood();
		}

		AddOffsetMicros(estimationErrorMicros);
	}

	/*
		Returns true if clock is ok.
	*/
	bool OnTuneErrorReceived(const int32_t estimationErrorMicros)
	{
		AddOffsetMicros(estimationErrorMicros);

		if (abs(estimationErrorMicros) < MAX_TUNE_ERROR_MICROS)
		{
			StampSyncGood();
			StampSynced();

			return true;
		}		

		return false;
	}
};

class LinkHostClockSyncer : public LoLaLinkClockSyncer
{
protected:
	void OnReset()
	{
		SetRandom();
	}

public:
	LinkHostClockSyncer() : LoLaLinkClockSyncer() {}

	bool IsSynced()
	{
		return SyncGoodCount > LOLA_LINK_SERVICE_UNLINK_MIN_CLOCK_SAMPLES;
	}

	bool OnEstimationReceived(const int32_t estimationErrorMicros)
	{
		if (abs(estimationErrorMicros) < MAX_TUNE_ERROR_MICROS)
		{
			StampSyncGood();
			return true;
		}
		else if (!IsSynced())
		{
			//Only reset the sync good count when syncing.
			SyncGoodCount = 0;
		}

		return false;
	}
};

#endif