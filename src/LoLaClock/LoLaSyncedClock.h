// LoLaSyncedClock.h

#ifndef _LOLA_SYNCED_CLOCK_h
#define _LOLA_SYNCED_CLOCK_h

#include <LoLaClock\LoLaClock.h>
#include <Arduino.h>
#include <LoLaClock\FreeRunningTimer.h>

class LoLaSyncedClock : public ISyncedClock
{
private:
	static const uint32_t MinTimerRange = 10000;

protected:
	// Clock Timer.
	FreeRunningTimer TimerSource;
	static const uint32_t TimerRange = FreeRunningTimer::TimerRange;

	// Live time data.
	volatile uint32_t SecondsStep = 0;
	volatile uint32_t StepsPerSecond = TimerRange;
	int32_t OffsetMicros = 0;

public:
	LoLaSyncedClock()
		: TimerSource()
		, ISyncedClock()
	{
	}

	// Setup a interrupt callback to OnTick(), to train the timer on the accurate 1 PPS signal.
	bool Setup()
	{
		if (TimerRange > MinTimerRange && TimerSource.SetupTimer())
		{
			return true;
		}

		return false;
	}

	virtual const void GetSyncSecondsMillis(uint32_t& seconds, uint16_t& millis)
	{
		// Capture pair of values, guaranteed cohesion.
		// Minimal interrupt disruption, only 2 copies.
		uint32_t StepHelper;

		noInterrupts();
		seconds = SecondsCounter;
		StepHelper = TimerSource.GetStep();
		interrupts();

		if (StepHelper < SecondsStep)
		{
			StepHelper = ((TimerRange - SecondsStep) + StepHelper);
		}
		else
		{
			StepHelper = (StepHelper - SecondsStep);
		}

		millis = (StepHelper * (ONE_SECOND_MILLIS / StepsPerSecond)) % ONE_SECOND_MILLIS;
		//millis = (((uint64_t)StepHelper * ONE_SECOND_MILLIS) / StepsPerSecond) % ONE_SECOND_MILLIS;
	}

	virtual const uint64_t GetSyncMicrosFull()
	{
		// Capture pair of values, guaranteed cohesion.
		// Minimal interrupt disruption, only 2 copies.
		uint32_t SecondsHelper;
		uint32_t StepHelper;

		noInterrupts();
		SecondsHelper = SecondsCounter;
		StepHelper = TimerSource.GetStep();
		interrupts();

		if (StepHelper < SecondsStep)
		{
			StepHelper = ((TimerRange - SecondsStep) + StepHelper);
		}
		else
		{
			StepHelper = (StepHelper - SecondsStep);
		}

		// Start with seconds...
		uint64_t Result = (uint64_t)SecondsHelper * ONE_SECOND_MICROS;

		// Add running micros from timer....
		Result += (((uint64_t)StepHelper * ONE_SECOND_MICROS) / StepsPerSecond);

		// End with micros offset.
		Result += OffsetMicros;


		return Result;
	}

	virtual void SetOffsetSeconds(const uint32_t offsetSeconds)
	{
		SecondsCounter = offsetSeconds;
	}

	virtual void AddOffsetMicros(const int32_t offsetMicros)
	{
		OffsetMicros += offsetMicros;
		PruneOffsetMicros();
	}

private:
	// Removes any extra second from the offset and transfers it to the SecondsCounter.
	void PruneOffsetMicros()
	{
		while (true)
		{
			if (OffsetMicros >= ONE_SECOND_MICROS)
			{
				SecondsCounter += 1;
				OffsetMicros -= ONE_SECOND_MICROS;
			}
			else if (OffsetMicros <= -ONE_SECOND_MICROS)
			{
				SecondsCounter -= 1;
				OffsetMicros += ONE_SECOND_MICROS;
			}
			else
			{
				break;
			}
		}
	}
};
#endif