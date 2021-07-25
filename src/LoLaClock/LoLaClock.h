// LoLaClock.h

#ifndef _LOLA_CLOCK_h
#define _LOLA_CLOCK_h

#include <stdint.h>

struct IClock
{
public:
	static const uint32_t ONE_SECOND_MICROS = 1000000;
	static const uint32_t ONE_SECOND_NANOS = 1000000000;
	static const uint32_t ONE_SECOND_MILLIS = 1000;
	static const uint32_t ONE_MILLI_MICROS = 1000;
	static const uint32_t ONE_SECOND = 1;
};

// Virtual interface for a target to fired from an interrupt.
class ISyncedCallbackTarget : public IClock
{
public:
	ISyncedCallbackTarget() : IClock() {}

	virtual void InterruptCallback() {}
};

class ISyncedCallbackSource : public IClock
{
public:
	ISyncedCallbackSource() : IClock() {}

	virtual void SetCallbackTarget(ISyncedCallbackTarget* source) {}

	virtual void StartCallbackAfterMicros(const uint32_t delayMicros) {}

	virtual void CancelCallback() {}
};


// Non-monotonic clock.
class ISyncedClock : public virtual ISyncedCallbackSource
{
protected:
	// Live time data.
	volatile uint32_t SecondsCounter = 0;

public:
	ISyncedClock() : ISyncedCallbackSource()
	{}

	// Rolls over every ~136 years.
	const uint32_t GetSyncSeconds()
	{
		return SecondsCounter;
	}

	// Rolls over every ~48 days.
	const uint32_t GetSyncMillis()
	{
		return (GetSyncMicrosFull() / ONE_MILLI_MICROS) % UINT32_MAX;
	}

	// Rolls over every ~70 minutes.
	const uint32_t GetSyncMicros()
	{
		return GetSyncMicrosFull() % UINT32_MAX;
	}

	// Rolls over every ~136 years.
	virtual const void GetSyncSecondsMillis(uint32_t& seconds, uint16_t& millis)
	{
		seconds = 0;
		millis = 0;
	}

	// Rolls over every ~136 years.
	virtual const uint64_t GetSyncMicrosFull()
	{
		return 0;
	}

	virtual bool HasTraining() { return false; }

	virtual void SetTuneOffset(const int32_t tuneOffset) {}

	virtual void SetOffsetSeconds(const int64_t offsetSeconds) {}

	virtual void AddOffsetMicros(const int32_t offsetMicros) {}
};

#endif