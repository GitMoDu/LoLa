// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <ClockSource.h>
#include <Services\Link\ClockSyncTransaction.h>
#include <Services\Link\LoLaLinkDefinitions.h>

class LoLaLinkClockSyncer
{
private:
	ClockSource * SyncedClock = nullptr;

protected:
	uint8_t SyncGoodCount = 0;
	uint32_t LastSynced = ILOLA_INVALID_MILLIS;

	static const int32_t MAX_TUNE_ERROR_MICROS = 10;

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
	bool Setup(ClockSource * syncedClock)
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
	virtual bool IsTimeToTune() { return false; };
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
	bool IsSynced()
	{
		return HasEstimation() && HostSynced;
	}

	bool IsTimeToTune()
	{
		return LastSynced == ILOLA_INVALID_MILLIS || ((millis() - LastSynced) > (LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD));
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
private:
	uint32_t LastGoodEstimation = ILOLA_INVALID_MILLIS;
	int32_t LastError = INT32_MAX;

protected:
	void OnReset()
	{
		LastGoodEstimation = ILOLA_INVALID_MILLIS;
		LastError = UINT32_MAX;

		SetRandom();
	}

public:
	int32_t GetLastError()
	{
		return LastError;
	}

	bool IsSynced()
	{
		return SyncGoodCount > LOLA_LINK_SERVICE_UNLINK_MIN_CLOCK_SAMPLES;
	}

	void SetReadyForEstimation()
	{
		LastGoodEstimation = ILOLA_INVALID_MILLIS;
	}

	bool IsTimeToTune()
	{
		return IsSynced() && LastSynced != ILOLA_INVALID_MILLIS &&
			(LastGoodEstimation == ILOLA_INVALID_MILLIS ||
			(millis() - LastGoodEstimation) > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD);
	}

	void OnEstimationReceived(const int32_t estimationError)
	{
		LastError = estimationError;
		if (abs(LastError) < MAX_TUNE_ERROR_MICROS)
		{
			LastGoodEstimation = millis();
			StampSyncGood();
		}
		else if (!IsSynced())
		{
			//Only reset the sync good count when syncing.
			SyncGoodCount = 0;
		}
	}
};

#endif