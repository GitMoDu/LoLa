// ClockSyncTransaction.h

#ifndef _CLOCKSYNCTRANSACTION_h
#define _CLOCKSYNCTRANSACTION_h

#include <stdint.h>

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
		Id = 0;
		Stage = 0;
		Result = 0;
		LastRequested = 0;
	}

	bool SetResult(const uint8_t requestId, const int32_t resultError)
	{
		if (Stage == TransactionStage::Requested && Id == requestId)
		{
			Result = resultError;
			Stage = TransactionStage::ResultIn;
			return true;
		}

		return false;
	}

	int32_t GetResult()
	{
		return Result;
	}

	void SetRequested()
	{
		Stage = TransactionStage::Requested;
		Id = random(0, UINT8_MAX);
		LastRequested = millis();
	}

	bool IsRequested()
	{
		return Stage == TransactionStage::Requested;
	}

	bool IsFresh(const uint32_t lifetimeMillis)
	{
		return millis() - LastRequested < lifetimeMillis;
	}

	bool IsResultWaiting()
	{
		return Stage == TransactionStage::ResultIn;
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

