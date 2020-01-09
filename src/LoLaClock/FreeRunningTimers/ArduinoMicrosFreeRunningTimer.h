// ArduinoMicrosFreeRunningTimer.h

#ifndef _ARDUINO_MICROS_FREERUNNINGTIMER_h
#define _ARDUINO_MICROS_FREERUNNINGTIMER_h


#include <LoLaClock\IClock.h>
#include <Arduino.h>


class ArduinoMicrosFreeRunningTimer : public virtual IFreeRunningTimer
{
public:
	FreeRunningTimer() : IFreeRunningTimer()
	{
	}

	virtual uint32_t GetCurrentStep() 
	{ 
		return micros(); 
	}

	virtual bool SetupTimer()
	{
		return true; 
	}
};
#endif