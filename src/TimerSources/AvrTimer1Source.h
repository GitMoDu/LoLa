// AvrTimer1Source.h

#ifndef _ARDUINO_TIMER_1_SOURCE_h
#define _ARDUINO_TIMER_1_SOURCE_h

#include "ITimerSource.h"
#include <Arduino.h>

/// <summary>
/// AVR Timer 1 (16 bit) based timer source. 
/// TODO: Implement.
/// </summary>
class AvrTimer1Source : public virtual ITimerSource
{
public:
	AvrTimer1Source()
	{

	}

public:
	virtual const uint32_t GetCounter() final
	{
		return micros();
	}

	virtual const uint32_t GetDefaultRollover() final
	{
		return 1000000;
	}

	virtual void StartTimer() final
	{
	}

	virtual void StopTimer() final
	{}
};
#endif