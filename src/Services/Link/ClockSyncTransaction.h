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

class ClockSecondsRequestTransaction : public IClockSyncTransaction
{
private:
	enum TransactionStage : uint8_t
	{
		Requested = 1,
		ResultIn = 2,
		SecondsDone = 3
	};

	uint32_t Result = 0;
	uint32_t ResultMillisStamp = 0;
	uint32_t LastRequested = 0;

public:
	void Reset()
	{
		Id = 0;
		Stage = 0;
		ResultMillisStamp = 0;
		LastRequested = 0;
	}

	bool SetResult(const uint8_t requestId, const uint32_t resultUTC)
	{
		if (Stage == TransactionStage::Requested && Id == requestId)
		{
			Result = resultUTC;
			ResultMillisStamp = millis();
			Stage = TransactionStage::ResultIn;
			return true;
		}

		return false;
	}

	uint32_t GetResult()
	{
		return Result + ((millis() - ResultMillisStamp) / 1000);
	}

	bool IsRequested()
	{
		return Stage == TransactionStage::Requested;
	}

	bool IsResultWaiting()
	{
		return Stage == TransactionStage::ResultIn;
	}

	bool IsDone()
	{
		return Stage == TransactionStage::SecondsDone;
	}

	bool IsFresh(const uint32_t lifetimeMillis)
	{
		return (millis() - LastRequested) < lifetimeMillis;
	}

	void SetRequested()
	{
		Stage = TransactionStage::Requested;
		Id = random(0, UINT8_MAX);
		LastRequested = millis();
	}

	void SetDone()
	{
		Stage = TransactionStage::SecondsDone;
	}
};

class ClockSecondsResponseTransaction : public IClockSyncTransaction
{
private:
	enum TransactionStage : uint8_t
	{
		Requested = 1
	};

public:
	bool IsRequested()
	{
		return Stage == TransactionStage::Requested;
	}

	void SetRequested(const uint8_t requestId)
	{
		Id = requestId;
		Stage = TransactionStage::Requested;
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
		return (millis() - LastRequested) < lifetimeMillis;
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

	void SetResult(const uint8_t requestId, const int32_t estimationError)
	{
		Id = requestId;
		Result = estimationError;
		Stage = TransactionStage::ResultIn;
	}

	int32_t GetResult()
	{
		return Result;
	}
};

#endif

