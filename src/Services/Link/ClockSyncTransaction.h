// ClockSyncTransaction.h

#ifndef _CLOCKSYNCTRANSACTION_h
#define _CLOCKSYNCTRANSACTION_h

#include <stdint.h>
#include <Services\Link\LoLaLinkDefinitions.h>

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
		return millis() - LastRequested < LOLA_LINK_SERVICE_UNLINK_TRANSACTION_LIFETIME;
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

