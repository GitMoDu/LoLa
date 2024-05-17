// Esp32TimerCycles.h
#ifndef _ESP32_TIMER_CYCLES_h
#define _ESP32_TIMER_CYCLES_h

#if defined(ARDUINO_ARCH_ESP32)

#include "ICycles.h"
#include <esp32-hal-timer.h>


/// <summary>
/// ESP32 timer cycle counter.
/// </summary>
template<const uint8_t TimerIndex>
class Esp32TimerCycles : public ICycles
{
private:
	static constexpr uint16_t CLOCK_DIVISOR = APB_CLK_FREQ / 1000000;
private:
	hw_timer_t* TimerInstance = NULL;

public:
	Esp32TimerCycles() : ICycles()
	{}

public:
	const uint32_t GetCycles() final
	{
		return (uint32_t)timerRead(TimerInstance);
	}

	const uint32_t GetCyclesOverflow() final
	{
		return UINT32_MAX;
	}

	const uint32_t GetCyclesOneSecond()  final
	{
		return ONE_SECOND_MICROS;
	}

	void StartCycles() final
	{
		if (TimerInstance == NULL)
		{
			TimerInstance = timerBegin(TimerIndex, CLOCK_DIVISOR, true);
		}
		if (!timerStarted(TimerInstance))
		{
			timerStart(TimerInstance);
		}
	}

	void StopCycles() final
	{
		if (TimerInstance != NULL)
		{
			if (timerStarted(TimerInstance))
			{
				timerStop(TimerInstance);
			}
			timerEnd(TimerInstance);

			TimerInstance = NULL;
		}
	}
};
#endif
#endif