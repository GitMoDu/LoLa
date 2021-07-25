// ArduinoMicrosFreeRunningTimer.h

#ifndef _ARDUINO_MICROS_FREERUNNINGTIMER_h
#define _ARDUINO_MICROS_FREERUNNINGTIMER_h


#include <LoLaClock\LoLaClock.h>
#include <Arduino.h>


class FreeRunningTimer
{
public:
	static const uint32_t TimerRange = UINT32_MAX;

public:
	FreeRunningTimer()
	{
	}

	void SetCallbackTarget(ISyncedCallbackTarget* source)
	{
	}

	void StartCallbackAfterSteps(const uint32_t steps)
	{

	}

	uint32_t GetStep() 
	{ 
		return micros(); 
	}

	bool SetupTimer()
	{
		return true; 
	}
};
#endif