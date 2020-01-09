// IClock.h

#ifndef _ISYNCEDCLOCK_h
#define _ISYNCEDCLOCK_h


#include <stdint.h>

class IClock
{
public:
	static const uint32_t ONE_SECOND_MICROS = 1000000;
	static const uint32_t ONE_SECOND_MILLIS = 1000;
	static const uint32_t ONE_MILLI_MICROS = 1000;
	static const uint32_t ONE_SECOND = 1;

	static const uint32_t TOLERANCE_MICROS = 100;
	static const uint32_t ONE_SECOND_WITH_TOLERANCE_MICROS = ONE_SECOND_MICROS + TOLERANCE_MICROS;
};


class IClockTickTarget : public IClock
{
public:
	IClockTickTarget() : IClock() {}

	virtual void OnTick() {}
	virtual void OnTrainingTick() {}
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
};


class ISyncedClock : public virtual IClockTickTarget
{
public:
	ISyncedClock() : IClockTickTarget()
	{}

	ISyncedCallbackSource* GetSyncedCallbackSource()
	{
		return nullptr;
	}

	// Rolls over every ~136 years.
	virtual uint32_t GetSyncSeconds()
	{
		return 0;
	}

	//// Rolls over every ~48 days.
	//virtual uint64_t GetSyncMillis()
	//{
	//	return 0;
	//}

	// Rolls over every ~584942 years.
	virtual uint64_t GetSyncMicros()
	{
		return 0;
	}

	void SetTuneOffset(const int32_t tuneOffset) {}

	void SetOffsetSeconds(const int64_t offsetSeconds) {}

	void SetOffsetMicros(const uint32_t offsetMicros) {}

	void AddOffsetMicros(const int32_t offsetMicros) {}
};

class IClockTickSource : public IClock
{
public:
	virtual void DetachAll() {}

	// Setup a interrupt callback to OnTick().
	virtual void SetupTickInterrupt() {}

	// Setup a temporary interrupt callback to OnTrainingTick(), just to train the timer on the accurate 1 PPS signal.
	virtual void SetupTrainingTickInterrupt() {}

	// tuneOffset in ppb (Parts per Billion)
	virtual void SetTuneOffset(const int32_t tunePPB) {}
};

// 
class IClockMicrosSource : public virtual ISyncedCallbackSource
{
public:
	IClockMicrosSource() : ISyncedCallbackSource() {}

	virtual uint32_t GetMicros()
	{
		return 0;
	}

	virtual void OnTuneUpdated() {}
};

// Virtual interface for a free running, self resetting timer hardware.
class IFreeRunningTimer : public IClock
{
public:
	IFreeRunningTimer() : IClock() {}

	// Should set up the timer with >20000 per calibrated second.
	// With a period of > 1,1 seconds.
	virtual bool SetupTimer() { return false; }

	virtual uint32_t GetCurrentStep() { return 0; }

	virtual void SetCallbackTarget(ISyncedCallbackTarget* source) {}

	virtual void StartCallbackAfterSteps(const uint32_t steps) {}
};
#endif