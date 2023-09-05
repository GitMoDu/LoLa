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
/// Uses task callback to apply tune and ensure micros() overflows are tracked.
/// </summary>
class ArduinoTaskTimerClockSource : private Task, public virtual IClockSource, public virtual ITimerSource
{
private:
	uint32_t LastMicros = 0;
	uint32_t LastSmear = 0;

	uint32_t Overflows = 0;
	uint32_t TuneOffset = 0;

	int16_t TuneMicros = 0;

public:
	ArduinoTaskTimerClockSource(Scheduler& scheduler)
		: IClockSource()
		, ITimerSource()
		, Task(0, TASK_FOREVER, &scheduler, false)
	{}

public:
	/// <summary>
	/// Task callback override.
	/// </summary>
	/// <returns></returns>
	bool Callback() final
	{
		const uint32_t smearDelay = CheckSmearAdaptive();
		const uint32_t overflowDelay = CheckOverflows();

		uint32_t waitDelay = smearDelay;
		if (overflowDelay < smearDelay)
		{
			waitDelay = overflowDelay;
		}

		if (waitDelay > 0)
		{
			Task::delay(waitDelay);
		}
		else
		{
			Task::enable();
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
		return micros() + TuneOffset;
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
	virtual const bool StartClock(IClockSource::IClockListener* tickListener, const int16_t ppm) final
	{
		TuneMicros = ppm;

		if (!Task::isEnabled())
		{
			LastMicros = micros();
			LastSmear = LastMicros;

			Task::enable();
		}

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
	/// </summary>
	virtual void StopClock() final
	{
		Task::disable();
	}

	/// <summary>
	/// IClockSource override.
	/// Arduino Clock Source is tuneable up to +-CLOCK_TUNE_RANGE_MICROS.
	/// </summary>
	/// <param name="ppm"></param>
	virtual void TuneClock(const int16_t ppm) final
	{
		if (Task::isEnabled())
		{
			if (ppm != TuneMicros)
			{
				TuneMicros = ppm;

				Task::enable();
			}
			else
			{
				Task::enableIfNot();
			}
		}
	}

private:
	const uint32_t CheckOverflows()
	{
		const uint32_t timestamp = micros();

		const uint32_t nextOverflowPeriod = UINT32_MAX - timestamp;

		// Check micros() for rollover and compensate in overflows.
		if (timestamp < LastMicros)
		{
			Overflows++;
			LastMicros = timestamp;

#if defined(DEBUG_LOLA)
			Serial.println(F("\tmicros() overflow detected."));
#endif
			return 0;
		}
		else if (nextOverflowPeriod < ONE_SECOND_MICROS)
		{
			if (nextOverflowPeriod > ONE_MILLI_MICROS)
			{
				// Sleep until we're one milisecond away from overflow.
				return (nextOverflowPeriod - ONE_MILLI_MICROS) / ONE_MILLI_MICROS;
			}
			else
			{
				// Rollover is imminent.
				return 0;
			}
		}
		else
		{
			// Sleep until we're one second away from overflow.
			return ((nextOverflowPeriod - ONE_SECOND_MICROS) / ONE_MILLI_MICROS);
		}
	}

	const uint32_t GetAdaptiveSmearPeriod()
	{
		if (TuneMicros >= LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
		{
			return ONE_SECOND_MILLIS / LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS;
		}
		else if (TuneMicros <= -LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
		{
			return ONE_SECOND_MILLIS / LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS;
		}
		else if (TuneMicros > 0)
		{
			return ONE_SECOND_MILLIS / TuneMicros;
		}
		else if (TuneMicros < 0)
		{
			return ONE_SECOND_MILLIS / -TuneMicros;
		}
		else
		{
			return ONE_SECOND_MILLIS;
		}
	}

	/// <summary>
	/// Dynamic division up CLOCK_TUNE_RANGE_MICROS smears per second.
	/// </summary>
	/// <returns></returns>
	const uint32_t CheckSmearAdaptive()
	{
		if (TuneMicros != 0)
		{
			const uint32_t timestamp = millis();
			const uint32_t smearPeriod = GetAdaptiveSmearPeriod();

			if (timestamp - LastSmear >= smearPeriod)
			{
				LastSmear = timestamp;

				// Tune up/down, check TuneOffset for rollover
				//  and compensate in overflows.
				const uint32_t preTune = TuneOffset;
				if (TuneMicros > 0)
				{
					if (TuneOffset == INT32_MAX)
					{
						Overflows++;
					}
					TuneOffset++;
				}
				else if (TuneMicros < 0)
				{
					if (TuneOffset == 0)
					{
						Overflows--;
					}
					TuneOffset--;
				}

				return smearPeriod;
			}
			else if (timestamp - LastSmear < smearPeriod)
			{
				return smearPeriod - (timestamp - LastSmear);
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return UINT32_MAX;
		}
	}
};
#endif