// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <ClockSource.h>

#define CLOCK_SYNC_REQUEST_TRANSACTION_MAX_DURATION_MILLIS 300
#define CLOCK_SYNC_GOOD_ENOUGH_COUNT						2

#define CLOCK_SYNC_TUNE_ELAPSED_MILLIS						10000
#define CLOCK_SYNC_MAX_TUNE_ERROR							5
#define CLOCK_SYNC_TUNE_REMOTE_PREENTIVE_PERIOD_MILLIS		1000

class ILinkClockSyncer
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

	void AddOffset(const int32_t offset)
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

class LinkRemoteClockSyncer : public ILinkClockSyncer
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
		return LastSynced == 0 || ((millis() - LastSynced) > (CLOCK_SYNC_TUNE_ELAPSED_MILLIS - CLOCK_SYNC_TUNE_REMOTE_PREENTIVE_PERIOD_MILLIS));
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

	void OnEstimationErrorReceived(const int32_t estimationError)
	{
		if (estimationError == 0)
		{
			StampSyncGood();
		}
		else
		{
			AddOffset(estimationError);
		}
	}
};

class LinkHostClockSyncer : public ILinkClockSyncer
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

	//TODO: ClockSyncWarning
	//bool IsTimeToTune()
	//{
	//	return LastSynced == 0 || !IsSynced() || ((millis() - LastSynced) > CLOCK_SYNC_TUNE_ELAPSED_MILLIS);
	//}

	void OnEstimationReceived(const int32_t estimationError)
	{
		LastError = estimationError;
		if (LastError == 0)
		{
			StampSyncGood();
		}
		else if(!IsSynced() && (LastError > 1))
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