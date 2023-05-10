// ArduinoTaskTimerClockSource.h

#ifndef _ARDUINO_TASK_TIMER_CLOCK_SOURCE_h
#define _ARDUINO_TASK_TIMER_CLOCK_SOURCE_h


#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "..\IClockSource.h"
#include "..\ITimerSource.h"
#include "..\Clock\Timestamp.h"
#include <Arduino.h>

/// <summary>
/// Arduino micros() mixed timer and clock source.
/// Uses task callback to ensure micros() overflows are tracked.
/// Not tuneable.
/// </summary>
template<const uint32_t TimerOffset = 0>
class ArduinoTaskTimerClockSource : private Task, public virtual IClockSource, public virtual ITimerSource
{
private:
	static constexpr uint32_t CHECK_PERIOD = ONE_SECOND_MICROS;

private:
	uint32_t Overflows = 0;
	uint32_t LastMicros = 0;

public:
	ArduinoTaskTimerClockSource(Scheduler& scheduler)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, IClockSource()
		, ITimerSource()
	{}

public:
	/// <summary>
	/// Task callback override.
	/// </summary>
	/// <returns></returns>
	bool Callback() final
	{
		// Immediatelly check on resume.
		CheckOverflows();

		// How long until next overflow?
		const uint32_t nextOverflowPeriod = UINT32_MAX - micros();

		if (nextOverflowPeriod > (CHECK_PERIOD * 2))
		{
			// >2xLONG_CHECK_PERIOD to next overflow, sleep until we're 1xLONG_CHECK_PERIOD away.
			Task::delay((nextOverflowPeriod - (CHECK_PERIOD * 2)) / ONE_MILLI_MICROS);
		}
		else if (nextOverflowPeriod > CHECK_PERIOD)
		{
			// >LONG_CHECK_PERIOD to next overflow, sleep until we're half-way there.
			Task::delay((nextOverflowPeriod - CHECK_PERIOD) / ONE_MILLI_MICROS);
		}
		else
		{
			// Near overflow, check every second.
			Task::delay(CHECK_PERIOD);
		}

		return true;
	}

public:
	/// <summary>
	/// ITimerSource override.
	/// </summary>
	/// <returns></returns>
	virtual const uint32_t GetCounter() final
	{
		return micros() + TimerOffset;
	}

	/// <summary>
	/// ITimerSource override.
	/// </summary>
	/// <returns></returns>
	virtual const uint32_t GetDefaultRollover() final
	{
		return ONE_SECOND_MICROS;
	}

	/// <summary>
	/// TimerSource override.
	/// Arduino micros() timer is always running.
	/// </summary>
	virtual void StartTimer() final
	{
		Task::enable();
	}

	/// <summary>
	/// TimerSource override.
	/// Arduino micros() timer is always running.
	/// </summary>
	virtual void StopTimer() final
	{}

public:
	/// <summary>
	/// IClockSource override.
	/// </summary>
	/// <returns></returns>
	virtual const bool StartClock(const int16_t ppm) final
	{
		Task::enable();

		return true;
	}
	/// <summary>
	/// IClockSource override.
	/// </summary>
	/// <returns></returns>
	virtual const uint32_t GetTicklessOverflows() final
	{
		CheckOverflows();

		return Overflows;
	}

	/// <summary>
	/// IClockSource override.
	/// No clock tick interrupt, micros().
	/// </summary>
	/// <returns></returns>
	virtual const bool ClockHasTick() final
	{
		return false;
	}

	/// <summary>
	/// IClockSource override.
	/// </summary>
	virtual void StopClock() final
	{
		Task::disable();
	}

	/// <summary>
	/// IClockSource override.
	/// Arduino Clock Source is not tuneable.
	/// </summary>
	/// <param name="ppm"></param>
	virtual void TuneClock(const int16_t ppm) final
	{
		Task::enable();
	}

private:
	void CheckOverflows()
	{
		const uint32_t timestamp = micros();

		if (timestamp < LastMicros)
		{
			Overflows++;
#if defined(DEBUG_LOLA)
			Serial.println(F("\tMicros overflow detected."));
#endif
		}

		LastMicros = timestamp;
	}
};
#endif