// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <ClockSource.h>

#define CLOCK_SYNC_REQUEST_TRANSACTION_MAX_DURATION_MILLIS	(uint32_t)300
#define CLOCK_SYNC_GOOD_ENOUGH_COUNT						2

#define CLOCK_SYNC_TUNE_ELAPSED_MILLIS						(uint32_t)10000
#define CLOCK_SYNC_MAX_TUNE_ERROR							100
#define CLOCK_SYNC_TUNE_REMOTE_PREENTIVE_PERIOD_MILLIS		(uint32_t)1000

#define LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR				(int8_t)4
#define LOLA_CLOCK_SYNC_TUNE_RATIO							(int8_t)2

#define LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MAX				(INT8_MAX - LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR)
#define LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MIN				(INT8_MIN + LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR)
#define LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_DIVISOR			(LOLA_CLOCK_SYNC_TUNE_RATIO * LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR)


class LoLaLinkClockSyncer
{
private:
	ClockSource * SyncedClock = nullptr;

protected:
	uint8_t SyncGoodCount = 0;
	uint32_t LastSynced = 0;

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

	inline int32_t GetOffset()
	{
		if (SyncedClock != nullptr)
		{
			return SyncedClock->GetOffset();
		}

		return 0;
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

	uint32_t GetMillisSync()
	{
		if (SyncedClock != nullptr)
		{
			return SyncedClock->GetMillis();
		}
		return 0;
	}

	uint32_t GetMillisSynced(const uint32_t sourceMillis)
	{
		if (SyncedClock != nullptr)
		{
			return SyncedClock->GetMillisSynced(sourceMillis);
		}

		return 0;
	}

	void Reset()
	{
		SyncGoodCount = 0;
		LastSynced = 0;
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
		return LastSynced == 0 || ((millis() - LastSynced) > (CLOCK_SYNC_TUNE_ELAPSED_MILLIS - CLOCK_SYNC_TUNE_REMOTE_PREENTIVE_PERIOD_MILLIS));
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

	void OnTuneErrorReceived(const int32_t estimationError)
	{
		if (abs(estimationError > CLOCK_SYNC_MAX_TUNE_ERROR))
		{
			//TODO: Count sequential high error tune results, break connection on threshold value.
			return;
		}
		else if (estimationError == 0)
		{
			ClockTuneAccumulator = 0;
			StampSynced();
		}
		else
		{
			if (estimationError > 0)
			{
				if (ClockTuneAccumulator < LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MAX)
				{
					ClockTuneAccumulator += LOLA_CLOCK_SYNC_TUNE_ALIASING_FACTOR;
				}
				else 
				{
					AddOffset(LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_MAX/ LOLA_CLOCK_SYNC_TUNE_ACCUMULATOR_DIVISOR);
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
	}
};

class LinkHostClockSyncer : public LoLaLinkClockSyncer
{
private:
	uint32_t LastEstimation = 0;
	int32_t LastError = INT32_MAX;

protected:
	void OnReset()
	{
		LastEstimation = 0;
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
		return SyncGoodCount >= CLOCK_SYNC_GOOD_ENOUGH_COUNT;
	}

	bool IsTimeToTune()
	{
		return IsSynced() && LastSynced != 0 &&
			(LastEstimation == 0 ||
			(millis() - LastEstimation) > CLOCK_SYNC_TUNE_ELAPSED_MILLIS);
	}

	void OnEstimationReceived(const int32_t estimationError)
	{
		LastEstimation = millis();
		LastError = estimationError;
		if (LastError == 0)
		{
			StampSyncGood();
		}
		else if (!IsSynced() && (LastError > 1))
		{
			SyncGoodCount = 0;
		}
	}
};

class IClockSyncTransaction
{
protected:
	uint8_t Stage = 0;
	uint8_t Id = 0;

public:
	virtual void Reset()
	{
		Id = 0;
		Stage = 0;
	}

	bool IsClear()
	{
		return Stage == 0;
	}

	uint8_t GetId()
	{
		return Id;
	}
};


class ClockSyncRequestTransaction : public IClockSyncTransaction
{
private:
	uint32_t LastRequested = 0;
	int32_t Result = 0;

	enum TransactionStage : uint8_t
	{
		Requested = 1,
		ResultIn = 2
	};

public:
	void Reset()
	{
		Id--;
		Stage = 0;
		Result = 0;
		LastRequested = 0;
	}

	void SetResult(const int32_t resultError)
	{
		Result = resultError;
		Stage = TransactionStage::ResultIn;
	}

	int32_t GetResult()
	{
		return Result;
	}

	void SetRequested()
	{
		Stage = TransactionStage::Requested;
		LastRequested = millis();
	}

	bool IsRequested()
	{
		return Stage == TransactionStage::Requested && IsFresh();
	}

	bool IsFresh()
	{
		return millis() - LastRequested < CLOCK_SYNC_REQUEST_TRANSACTION_MAX_DURATION_MILLIS;
	}

	bool IsResultWaiting()
	{
		return Stage == TransactionStage::ResultIn;
	}

	void SetResultWaiting()
	{
		if (Stage == TransactionStage::Requested)
		{
			Stage = TransactionStage::ResultIn;
		}
	}
};

class ClockSyncResponseTransaction : public IClockSyncTransaction
{
private:
	int32_t Result = 0;

	enum TransactionStage : uint8_t
	{
		ResultIn = 1
	};
public:
	bool IsResultReady()
	{
		return Stage == TransactionStage::ResultIn;
	}

	void SetResult(const uint8_t requestId, const int32_t resultError)
	{
		Id = requestId;
		Result = resultError;
		Stage = TransactionStage::ResultIn;
	}

	int32_t GetResult()
	{
		return Result;
	}
};
#endif