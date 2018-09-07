// LoLaLinkClockSyncer.h

#ifndef _LOLALINKCLOCKSYNCER_h
#define _LOLALINKCLOCKSYNCER_h

#include <ClockSource.h>

#define CLOCK_SYNC_REQUEST_TRANSACTION_MAX_DURATION_MILLIS 300
#define CLOCK_SYNC_GOOD_ENOUGH_COUNT 3

class LoLaLinkClockSyncer
{
private:
	ClockSource * SyncedClock = nullptr;
	uint32_t Helper;

	uint8_t SyncGoodCount = 0;


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

	void StampSyncGood()
	{
		if (SyncGoodCount < UINT8_MAX)
		{
			SyncGoodCount++;
		}
	}

	//Only for Host clock.
	void OnEstimationReceived(const uint32_t absoluteMillis)
	{
		if(SyncedClock->GetMillis() - absoluteMillis == 0)
		{
			StampSyncGood();
		}
		else 
		{
			//Either we get CLOCK_SYNC_GOOD_ENOUGH_COUNT in a row or we start again.
			SyncGoodCount = 0;
		}
	}

	//Only for Remote clock.
	void OnEstimationErrorReceived(const uint32_t estimationError)
	{
		if (estimationError == 0)
		{
			StampSyncGood();
		}
		else
		{
			//Either we get CLOCK_SYNC_GOOD_ENOUGH_COUNT in a row or we start again.
			SyncGoodCount = 0;
		}
	}

	uint32_t Sync(const uint32_t absoluteMillis)
	{
		if (SyncedClock != nullptr)
		{
			Helper = SyncedClock->GetMillis() - absoluteMillis;

			SyncedClock->AddOffset(Helper);

			return Helper;
		}

		return UINT32_MAX;
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