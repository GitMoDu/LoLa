// TuneClock.h

#ifndef _TUNE_CLOCK_h
#define _TUNE_CLOCK_h

#include "TimeClock.h"

/// <summary>
/// Tuneable real time clock.
/// Rolls over after ~136.1 years.
/// Tracks Cyclestamp overflow count.
/// Uses task callback to smear the tune offset, 
///  as well as ensure cyclestamp' overflows are tracked.
/// Task callback delays the shortest required period for each callback pass.
/// </summary>
class TuneClock : public TimeClock
{
private:
	uint32_t LastSmear = 0;

	int8_t TuneMicros = 0;

public:
	TuneClock(TS::Scheduler& scheduler, ICycles* cycles)
		: TimeClock(scheduler, cycles)
	{
	}

	/// <summary>
	/// Start the clock.
	/// Will use the existing clock tune.
	/// </summary>
	void Start()
	{
		LastSmear = 0;

		TimeClock::Start();
	}

	/// <summary>
	/// Start the clock with a set tune.
	/// </summary>
	/// <param name="ppm">Starting tune in microseconds/second: [INT8_MIN;INT8_MAX]</param>
	void Start(const int8_t ppm)
	{
		Start();

		SetTune(ppm);
	}

	/// <summary>
	/// Set the clock tune ppm.
	/// </summary>
	/// <param name="ppm">Tune in microseconds/second: [INT8_MIN;INT8_MAX]</param>
	void SetTune(const int8_t ppm)
	{
		if (TS::Task::isEnabled())
		{
			TuneMicros = ppm;
			TS::Task::enableDelayed(0);
		}
	}

	const int8_t GetTune() const
	{
		return TuneMicros;
	}

	/// <summary>
	/// Shifts the tune up and down..
	/// </summary>
	/// <param name="ppm">Tune shift in microseconds/second: [INT8_MIN;INT8_MAX]</param>
	void ShiftTune(const int8_t offsetMicros)
	{
		if (TS::Task::isEnabled())
		{
			const int8_t preTune = TuneMicros;

			int16_t tune = (int16_t)TuneMicros + offsetMicros;

			if (tune >= INT8_MAX)
			{
				TuneMicros = INT8_MAX;
			}
			else if (tune <= INT8_MIN)
			{
				TuneMicros = INT8_MIN;
			}
			else
			{
				TuneMicros = tune;
			}

			if (preTune != TuneMicros)
			{
				TS::Task::enableDelayed(0);
			}
		}
	}

protected:
	/// <summary>
	/// Checks for cyclestamp overflows.
	/// Smears the tune offset.
	/// </summary>
	/// <param name="cyclestamp"></param>
	/// <returns>Max delay in milliseconds until next run is required.</returns>
	uint32_t RunCheck(const uint32_t cyclestamp) final
	{
		const uint32_t overflowDelay = TimeClock::RunCheck(cyclestamp);
		const uint32_t smearDelay = CheckSmearAdaptive(cyclestamp);

		if (overflowDelay < smearDelay)
		{
			return overflowDelay;
		}
		else
		{
			return smearDelay;
		}
	}

private:
	/// <summary>
	/// </summary>
	/// <returns>Smear period based on the current tune value</returns>
	const uint32_t GetSmearPeriod() const
	{
		if (TuneMicros > 0)
		{
			return (int32_t)ONE_SECOND_MILLIS / TuneMicros;
		}
		else if (TuneMicros < 0)
		{
			return -(int32_t)ONE_SECOND_MILLIS / TuneMicros;
		}
		else
		{
			return ONE_SECOND_MILLIS;
		}
	}

	/// <summary>
	/// Dynamic division up INT8_MAX smears per second.
	/// </summary>
	/// <returns></returns>
	const uint32_t CheckSmearAdaptive(const uint32_t cyclestamp)
	{
		if (TuneMicros != 0)
		{
			const uint32_t elapsedSinceLastSmear = GetDurationCyclestamp(LastSmear, cyclestamp) / ONE_MILLI_MICROS;
			const uint32_t smearPeriod = GetSmearPeriod();

			if (elapsedSinceLastSmear >= smearPeriod)
			{
				LastSmear = cyclestamp;

				// Smear up/down. according to tune.
				if (TuneMicros > 0)
				{
					ShiftSubSeconds(+1);
				}
				else if (TuneMicros < 0)
				{
					ShiftSubSeconds(-1);
				}

				return 0;
			}
			else
			{
				return smearPeriod - elapsedSinceLastSmear;
			}
		}
		else
		{
			return UINT32_MAX;
		}
	}
};
#endif