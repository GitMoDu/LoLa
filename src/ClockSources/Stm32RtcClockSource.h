// Stm32RtcClockSource.h

#ifndef _STM32_RTC_CLOCK_SOURCE_h
#define _STM32_RTC_CLOCK_SOURCE_h

#include "..\IClockSource.h"
#include "..\Clock\AbstractClockSource.h"
#include "..\ITimerSource.h"
#include "..\Clock\Timestamp.h"
#include <Arduino.h>

/// <summary>
/// STM32 RTC clock source.
/// TODO: Implement.
/// </summary>
class Stm32RtcClockSource : public AbstractClockSource
{
private:
	using BaseClass = AbstractClockSource;

public:
	Stm32RtcClockSource(Scheduler& scheduler)
		: AbstractClockSource(scheduler)
	{}

public:
	virtual bool Callback() final
	{
		// Perform async operations.

		//TODO: Tuning.

		//TODO: Tuning smear.
		return false;
	}

	/// <summary>
	/// Arduino Micros timer is always on.
	/// </summary>
	/// <returns></returns>
	virtual const bool StartClock(IClockSource::IClockListener* tickListener, const int16_t ppm) final
	{
		if (BaseClass::StartClock(tickListener, ppm))
		{
			//TODO: Implement RTC callback.
			return false;

			return true;
		}

		return false;
	}

	/// <summary>
	/// //TODO: Implement
	/// </summary>
	virtual void StopClock() final
	{
	}

	/// <summary>
	/// //TODO: Implement
	/// </summary>
	/// <param name="ppm"></param>
	virtual void TuneClock(const int16_t ppm) final
	{}
};
#endif