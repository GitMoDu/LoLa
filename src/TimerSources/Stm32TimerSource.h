// Stm32TimerSource.h

#ifndef _STM32_TIMER_SOURCE_h
#define _STM32_TIMER_SOURCE_h

#include "ITimerSource.h"
#include <Arduino.h>

/// <summary>
/// Uses STM32 16 bit timer as timer source.
/// 16 bit timers are adequate but low resolution.
/// TODO: Implement.
/// </summary>
class Stm32TimerSource : public virtual ITimerSource
{
private:
	static const uint16_t TimerTolerance = UINT16_MAX / 10;
	static const uint16_t TopWithTolerance = UINT16_MAX - TimerTolerance;

public:
	Stm32TimerSource(const uint8_t timerIndex, const uint8_t timerChannel)
	{}

public:
	virtual const uint32_t GetCounter() final
	{
		return 0;
	}

	virtual const uint32_t GetDefaultRollover() final
	{
		return UINT16_MAX;
	}

	virtual const uint32_t GetRollingMicros() final
	{
		//TODO: Get real timer.
		return 0;
	}
};
#endif