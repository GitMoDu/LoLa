// STM32FreeRunningTimer.h

#ifndef _STM32FREERUNNINGTIMER_h
#define _STM32FREERUNNINGTIMER_h


#include <LoLaDefinitions.h>
#include <LoLaClock\IClock.h>
#include <HardwareTimer.h>
#include <Arduino.h>


class FreeRunningTimer : public virtual IFreeRunningTimer
{
private:
	HardwareTimer Timer;

	static const uint8_t TimerIndex = LOLA_CLOCK_TIMER_INDEX;

	ISyncedCallbackTarget* CallbackTarget = nullptr;

	static const uint32_t TOLERANCE_MICROS = 20 * ONE_MILLI_MICROS;
	static const uint32_t ONE_SECOND_WITH_TOLERANCE_MICROS = ONE_SECOND_MICROS + TOLERANCE_MICROS;


private:
	const uint32_t GetPrescaleFactor()
	{
		uint64_t period_cyc = ONE_SECOND_WITH_TOLERANCE_MICROS * CYCLES_PER_MICROSECOND;

		return (uint16_t)((period_cyc / UINT16_MAX) + 1);
	}

public:
	// Defined on cpp.
	FreeRunningTimer();
	virtual void StartCallbackAfterSteps(const uint32_t steps);

public:
	virtual uint32_t GetTimerRange()
	{
		return UINT16_MAX;
	}

	virtual uint32_t GetStep()
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

	virtual void CancelCallback()
	{
		Timer.detachInterrupt(TimerIndex);
	}

	// Should set up the timer with >20000 steps per calibrated second.
	// With a period of > 1,1 seconds.
	virtual bool SetupTimer()
	{
		Timer.detachInterrupt(0);
		Timer.detachInterrupt(TimerIndex);
		Timer.pause();
		Timer.setPrescaleFactor(GetPrescaleFactor());
		Timer.setOverflow(UINT16_MAX);
		Timer.setMode(TimerIndex, timer_mode::TIMER_OUTPUT_COMPARE);
		Timer.refresh();
		Timer.resume();

		return true;
	}
};
#endif