// STM32FreeRunningTimer.h

#if !defined(_STM32FREERUNNINGTIMER_h) && defined(ARDUINO_ARCH_STM32F1)
#define _STM32FREERUNNINGTIMER_h


#include <LoLaDefinitions.h>
#include <LoLaClock\LoLaClock.h>
#include <HardwareTimer.h>
#include <Arduino.h>


class FreeRunningTimer : public IClock
{
public:
	static const uint32_t TimerRange = UINT16_MAX;
	static const uint16_t PreTrainedStepsPerSecond = 62000;

	volatile bool FreeRunningTimerConstructorOk = false;

private:
	static const uint8_t TimerIndex = 1;// Timer 1 does not conflict with SPI.
	static const uint8_t TimerChannelIndex = TIMER_CH1; // Pin (D27) PA8. 

	static const uint32_t TOLERANCE_MICROS = 50 * ONE_MILLI_MICROS; // 5% Tolerance on slow clock.
	static const uint32_t ONE_SECOND_WITH_TOLERANCE_MICROS = ONE_SECOND_MICROS + TOLERANCE_MICROS;

	HardwareTimer DeviceTimer;

	ISyncedCallbackTarget* CallbackTarget;

private:
	static const uint32_t GetPrescaleFactor()
	{
		uint64_t period_cyc = ONE_SECOND_WITH_TOLERANCE_MICROS * CYCLES_PER_MICROSECOND;

		return (uint16_t)((period_cyc / UINT16_MAX) + 1);
	}

public:
	FreeRunningTimer()
		: IClock()
		, DeviceTimer(TimerIndex)
		, CallbackTarget(nullptr)
	{
		FreeRunningTimerConstructorOk = true;
	}

	// Defined on cpp.
	virtual void StartCallbackAfterSteps(const uint16_t steps);

public:
	virtual uint32_t GetStep()
	{
		return DeviceTimer.getCount();
	}

	void OnCompareInterrupt()
	{
		DeviceTimer.detachInterrupt(TimerChannelIndex);
		CallbackTarget->InterruptCallback();
	}

	virtual void SetCallbackTarget(ISyncedCallbackTarget* target)
	{
		if (target != nullptr)
		{
			CallbackTarget = target;
		}
	}

	virtual void CancelCallback()
	{
		Detach();
	}

	// Should set up the timer with >20000 steps per calibrated second.
	// With a period of over 1 second, to catch slow clocks.
	bool SetupTimer()
	{
		CallbackTarget = nullptr;

		SetupInterrupts();

		DeviceTimer.init();
		DeviceTimer.pause();
		DeviceTimer.detachInterrupt(TimerChannelIndex);
		DeviceTimer.setPeriod(ONE_SECOND_WITH_TOLERANCE_MICROS);
		DeviceTimer.setPrescaleFactor(GetPrescaleFactor());
		DeviceTimer.setOverflow(UINT16_MAX);
		DeviceTimer.setCount(0);
		DeviceTimer.setMode(TimerChannelIndex, timer_mode::TIMER_OUTPUT_COMPARE);
		DeviceTimer.refresh();
		DeviceTimer.resume();

		return true;
	}

private:
	// Defined on cpp.
	bool SetupInterrupts();

	void Detach()
	{
		DeviceTimer.detachInterrupt(TimerChannelIndex);
	}
};
#endif