// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <ClockSource.h>

#define CLOCK_SYNC_REQUEST_TRANSACTION_MAX_DURATION_MILLIS 300
#define CLOCK_SYNC_ESTIMATION_MIN_INTERVAL_MILLIS 50
#define CLOCK_SYNC_GOOD_ENOUGH_COUNT 3


class ILinkClockSyncer
{
private:
	ClockSource * SyncedClock = nullptr;
	uint8_t SyncGoodCount = 0;

protected:
	virtual void OnReset() {}

protected:
	void ResetSyncGoodCount()
	{
		SyncGoodCount = 0;
	}

	void StampSyncGood()
	{
		if (SyncGoodCount < UINT8_MAX)
		{
			SyncGoodCount++;
		}
	}

public:
	bool Setup(ClockSource * syncedClock)
	{
		SyncedClock = syncedClock;

		return SyncedClock != nullptr;
	}

	bool IsSynced()
	{
		return SyncGoodCount > CLOCK_SYNC_GOOD_ENOUGH_COUNT;
	}

	uint32_t GetMillisSync()
	{
		if (SyncedClock != nullptr)
		{
			return SyncedClock->GetMillis();
		}
		return 0;
	}

	void Reset()
	{
		SyncGoodCount = 0;
		if (SyncedClock != nullptr)
		{
			SyncedClock->SetRandom();
		}
		OnReset();
	}
};

class LinkRemoteClockSyncer : public ILinkClockSyncer
{
private:

public:
	void OnEstimationErrorReceived(const uint32_t estimationError)
	{
		if (estimationError == 0)
		{
			StampSyncGood();
		}
		else
		{
			//Either we get CLOCK_SYNC_GOOD_ENOUGH_COUNT in a row or we start again.
			ResetSyncGoodCount();
		}
	}
};

class LinkHostClockSyncer : public ILinkClockSyncer
{
private:
	uint32_t LastEstimation = 0;
	uint32_t LastError = UINT32_MAX;

protected:
	void OnReset()
	{
		LastEstimation = 0;
		LastError = UINT32_MAX;
	}

public:
	uint32_t GetLastError()
	{
		return LastError;
	}

	void OnEstimationReceived(const uint32_t absoluteMillis)
	{
		LastError = GetMillisSync() - absoluteMillis;
		if (LastError == 0)
		{
			if (millis() - LastEstimation > CLOCK_SYNC_ESTIMATION_MIN_INTERVAL_MILLIS)
			{
				StampSyncGood();
			}
			else
			{
				Serial.println("ClockSyncer skipped an estimation");
			}
		}
		else
		{
			//Either we get CLOCK_SYNC_GOOD_ENOUGH_COUNT in a row or we start again.
			ResetSyncGoodCount();
		}
	}
};


class AbstractClockSyncTransaction
{
protected:
	enum TransactionStage : uint8_t
	{
		Clear = 0,
		Stage1 = 1,
		Stage2 = 2
	} Stage = TransactionStage::Clear;

	uint32_t Result = 0;

	uint8_t Id = 0;

public:
	bool IsClear()
	{
		return Stage == TransactionStage::Clear;
	}

	uint8_t GetId()
	{
		return Id;
	}

	uint32_t GetResult()
	{
		return Result;
	}

	virtual void Reset() {}
	virtual void SetResult(const uint32_t resultError) {}
};

class ClockSyncRequestTransaction : public AbstractClockSyncTransaction
{
private:
	uint32_t LastRequested = 0;

public:
	void Reset()
	{
		Id--;
		Stage = TransactionStage::Clear;
		Result = 0;
		LastRequested = 0;
	}

	void SetResult(const uint32_t resultError)
	{
		Result = resultError;
		Stage = TransactionStage::Stage2;
	}

	void SetRequested()
	{
		Stage = TransactionStage::Stage1;
		LastRequested = millis();
	}

	bool IsRequested()
	{
		return Stage == TransactionStage::Stage1 && millis() - LastRequested < CLOCK_SYNC_REQUEST_TRANSACTION_MAX_DURATION_MILLIS;
	}

	bool IsResultWaiting()
	{
		return Stage == TransactionStage::Stage2;
	}

	void SetResultWaiting()
	{
		if (Stage == TransactionStage::Stage1)
		{
			Stage = TransactionStage::Stage2;
		}
	}
};

class ClockSyncResponseTransaction : public AbstractClockSyncTransaction
{
public:
	void Reset()
	{
		Id = 0;
		Stage = TransactionStage::Clear;
		Result = 0;
	}

	bool IsResultReady()
	{
		return Stage == TransactionStage::Stage1;
	}

	void SetResult(const uint8_t requestId, const uint32_t resultError)
	{
		Id = requestId;
		Result = resultError;
		Stage = TransactionStage::Stage1;
	}
};
#endif