// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <ClockSource.h>
#include <Services\Link\ClockSyncTransaction.h>
#include <Services\Link\LoLaLinkDefinitions.h>


//#define CLOCK_SYNC_GOOD_ENOUGH_COUNT						2

//#define CLOCK_SYNC_TUNE_ELAPSED_MILLIS						(uint32_t)5000
#define LOLA_CLOCK_SYNC_MAX_TUNE_ERROR							(int32_t)100

#define LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR				(int8_t)4
#define LOLA_CLOCK_SYNC_TUNE_RATIO							(int8_t)3

#define LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MAX				(INT8_MAX - LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR)
#define LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MIN				(INT8_MIN + LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR)
#define LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_DIVISOR			(LOLA_CLOCK_SYNC_TUNE_RATIO * LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR)


class LoLaLinkClockSyncer
{
private:
	ClockSource * SyncedClock = nullptr;

protected:
	uint8_t SyncGoodCount = 0;
	uint32_t LastSynced = ILOLA_INVALID_MILLIS;

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

	inline void AddOffset(const int32_t offset)
	{
		if (SyncedClock != nullptr)
		{
			SyncedClock->AddOffset(offset);
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

	uint32_t GetMillisSynced(const uint32_t sourceMillis)
	{
		if (SyncedClock != nullptr)
		{
			return SyncedClock->GetSyncMillis(sourceMillis);
		}

		return 0;
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

	int8_t ClockTuneAccumulator = 0;

protected:
	void OnReset()
	{
		HostSynced = false;
		ClockTuneAccumulator = 0;
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
		ClockTuneAccumulator = 0;
	}

	bool HasEstimation()
	{
		return SyncGoodCount > 0;
	}

	void OnEstimationErrorReceived(const int32_t estimationError)
	{
		if (estimationError == 0)
		{
			StampSyncGood();
			ClockTuneAccumulator = 0;
		}
		else
		{
			AddOffset(estimationError);
		}
	}

	/*
	Returns false if tune was needed.
	*/
	bool OnTuneErrorReceived(const int32_t estimationError)
	{
		uint8_t Result = false;
		if (abs(estimationError) > LOLA_CLOCK_SYNC_MAX_TUNE_ERROR)
		{
			//TODO: Break connection on threshold value.
		}
		else
		{
			if (estimationError == 0)
			{
				ClockTuneAccumulator = 0;
				StampSynced();
				Result = true;
			}
			else if (estimationError > 0)
			{
				if (ClockTuneAccumulator < LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MAX)
				{
					ClockTuneAccumulator += LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR;
				}
				else
				{
					AddOffset(LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MAX / LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_DIVISOR);
					ClockTuneAccumulator = 0;
				}
			}
			else
			{
				if (ClockTuneAccumulator > LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MIN)
				{
					ClockTuneAccumulator -= LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR;
				}
				else
				{
					AddOffset(LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MIN / LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_DIVISOR);
					ClockTuneAccumulator = 0;
				}
			}

			AddOffset(ClockTuneAccumulator / LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_DIVISOR);
		}

		return Result;
	}
};

class LinkHostClockSyncer : public LoLaLinkClockSyncer
{
private:
	uint32_t LastEstimation = ILOLA_INVALID_MILLIS;
	int32_t LastError = INT32_MAX;

protected:
	void OnReset()
	{
		LastEstimation = ILOLA_INVALID_MILLIS;
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
		return SyncGoodCount >= LOLA_LINK_SERVICE_UNLINK_MIN_CLOCK_SAMPLES;
	}

	void SetReadyForEstimation()
	{
		LastEstimation = ILOLA_INVALID_MILLIS;
	}

	bool IsTimeToTune()
	{
		return IsSynced() && LastSynced != ILOLA_INVALID_MILLIS &&
			(LastEstimation == ILOLA_INVALID_MILLIS ||
			(millis() - LastEstimation) > LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD);
	}

	void OnEstimationReceived(const int32_t estimationError)
	{
		LastEstimation = millis();
		LastError = estimationError;
		if (LastError == 0)
		{
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