// TickTrainedTimerSyncedClock.h

#ifndef _TICK_TRAINED_TIMER_SYNCED_CLOCK_h
#define _TICK_TRAINED_TIMER_SYNCED_CLOCK_h

#include <LoLaClock\LoLaSyncedClock.h>


class TickTrainedTimerSyncedClock
	: public LoLaSyncedClock
{
private:
	static const uint8_t MinSampleCount = 2;

	// Training data.
	volatile uint8_t SampleCount = 0;

	volatile uint32_t LastSecondsStep = 0;
	volatile uint32_t LastSecondCounter = 0;

public:
	TickTrainedTimerSyncedClock()
		: LoLaSyncedClock()
	{
	}

	bool HasTraining()
	{
		return SampleCount >= MinSampleCount;
	}

	bool Setup()
	{
		if (LoLaSyncedClock::Setup())
		{

			LastSecondsStep = 0;
			StepsPerSecond = TimerSource.PreTrainedStepsPerSecond;
			SampleCount = 0;


#ifdef DEBUG_LOLA_CLOCK_DEBUG_PIN
			pinMode(DEBUG_LOLA_CLOCK_DEBUG_PIN, OUTPUT);
			digitalWrite(DEBUG_LOLA_CLOCK_DEBUG_PIN, LOW);
#endif

			return true;
		}

		return false;
	}

	virtual void SetCallbackTarget(ISyncedCallbackTarget* source)
	{
		// Just pass it along.
		TimerSource.SetCallbackTarget(source);
	}

	virtual void StartCallbackAfterMicros(const uint32_t delayMicros)
	{
		// Just pass it along with converted steps from time.
		TimerSource.StartCallbackAfterSteps((((uint64_t)delayMicros * StepsPerSecond) / ONE_SECOND_MICROS) % UINT16_MAX);
	}

	// Called on tick interrupt. 
	// We piggyback the interrupt to update the training values.
	void OnTick()
	{
#ifdef DEBUG_LOLA_CLOCK_DEBUG_PIN
		digitalWrite(DEBUG_LOLA_CLOCK_DEBUG_PIN, !digitalRead(DEBUG_LOLA_CLOCK_DEBUG_PIN));
#endif
		// Increment the seconds counter, with rollover and get the current step count.
		// Interrupts don't need to be stopped because we're dealing with overflows.
		SecondsCounter++;
		SecondsStep = TimerSource.GetStep();
		//

		// Check sample count, need at least 2 samples (3 ticks).
		if (SampleCount < MinSampleCount)
		{
			SampleCount++;
		}
		else
		{
			// We can deal with one overflow between ticks, but no more.
			if (SecondsStep < LastSecondsStep)
			{
				// Timer rollover (overflow).
				StepsPerSecond = (((TimerRange - LastSecondsStep)) + SecondsStep);

			}
			else
			{
				StepsPerSecond = (SecondsStep - LastSecondsStep);
			}
		}

		LastSecondsStep = SecondsStep;
}

	const uint32_t GetStepDurationNanos()
	{
		return ONE_SECOND_NANOS / StepsPerSecond;
	}
};
#endif