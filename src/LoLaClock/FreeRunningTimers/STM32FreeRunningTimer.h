// STM32FreeRunningTimer.h

#ifndef _STM32FREERUNNINGTIMER_h
#define _STM32FREERUNNINGTIMER_h


#include <LoLaClock\IClock.h>
#include <LoLaDefinitions.h>
#include <HardwareTimer.h>
#include <Arduino.h>


class FreeRunningTimer : public virtual IFreeRunningTimer
{
private:
	HardwareTimer Timer;

	static const uint8_t TimerIndex = LOLA_CLOCK_TIMER_INDEX;

	ISyncedCallbackTarget* CallbackTarget = nullptr;

private:
	uint32_t GetPrescaleFactor()
	{
		uint32 period_cyc = ONE_SECOND_WITH_TOLERANCE_MICROS * CYCLES_PER_MICROSECOND;

		return (uint16)((period_cyc / UINT16_MAX) + 1);
	}

public:
	// Defined on cpp.
	FreeRunningTimer();
	virtual void StartCallbackAfterSteps(const uint32_t steps);

public:
	virtual uint32_t GetCurrentStep()
	{
		return Timer.getCount();
	}

	void OnCompareInterrupt()
	{
		Timer.detachInterrupt(TimerIndex);
		CallbackTarget->InterruptCallback();
	}

	virtual void SetCallbackTarget(ISyncedCallbackTarget* target)
	{
		CallbackTarget = target;
	}

	// Should set up the timer with >20000 steps per calibrated second.
	// With a period of > 1,1 seconds.
	virtual bool SetupTimer()
	{
		Timer.detachInterrupt(0);
		Timer.detachInterrupt(TimerIndex);
		Timer.refresh();
		Timer.setPrescaleFactor(GetPrescaleFactor());
		Timer.setOverflow(UINT16_MAX);
		Timer.setMode(TimerIndex, timer_mode::TIMER_OUTPUT_COMPARE);
		Timer.resume();

		return true;
	}
};
#endif