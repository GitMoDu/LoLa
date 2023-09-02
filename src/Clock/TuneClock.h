// TuneClock.h

#ifndef _TUNE_CLOCK_h
#define _TUNE_CLOCK_h

#include "TimeClock.h"

/// <summary>
/// Tuneable cycle clock.
/// Tracks Cyclestamp overflow count.
/// Uses task callback to smear the tune offset, 
///  as well as ensure cyclestamp' overflows are tracked.
/// Provides TunedTimestamp with overflows, based on cycle counts.
/// </summary>
class TuneClock : public TimeClock
{
private:
	uint32_t LastSmear = 0;

	int16_t TuneMicros = 0;

public:
	TuneClock(Scheduler& scheduler, ICycles* cycles)
		: TimeClock(scheduler, cycles)
	{}

	/// <summary>
	/// Start the clock.
	/// </summary>
	/// <param name="ppm">Starting tune in us.</param>
	void Start(const int16_t ppm)
	{
		LastSmear = 0;

		TimeClock::Start();

		Tune(ppm);
	}

	void Tune(const int16_t ppm)
	{
		if (Task::isEnabled())
		{
			const int_fast16_t preTune = TuneMicros;

			if (ppm > LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
			{
				TuneMicros = LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS;
			}
			else if (ppm < -LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
			{
				TuneMicros = -LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS;
			}
			else
			{
				TuneMicros = ppm;
			}
			if (preTune != TuneMicros)
			{
				Task::enable();
			}
		}
	}

	/// <summary>
	/// Clock is tuneable up to +-CLOCK_TUNE_RANGE_MICROS.
	/// </summary>
	/// <param name="ppm"></param>
	void ShiftTune(const int16_t offsetMicros)
	{
		if (Task::isEnabled())
		{
			const int_fast16_t preTune = TuneMicros;
			if (offsetMicros > 0)
			{
				if ((int32_t)TuneMicros + offsetMicros < LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
				{
					TuneMicros += offsetMicros;
				}
				else
				{
					TuneMicros = LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS;
				}
			}
			else if (offsetMicros < 0)
			{
				if ((int32_t)TuneMicros + offsetMicros > -LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
				{
					TuneMicros += offsetMicros;
				}
				else
				{
					TuneMicros = -LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS;
				}
			}

			if (preTune != TuneMicros)
			{
				Task::enable();
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
	virtual uint32_t RunCheck(const uint32_t cyclestamp) final
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
	const uint32_t CheckSmearAdaptive(const uint32_t cyclestamp)
	{
		if (TuneMicros != 0)
		{
			const uint32_t elapsedSinceLastSmear = GetDurationCyclestamp(LastSmear, cyclestamp);
			const uint32_t smearPeriod = GetAdaptiveSmearPeriod();

			if (elapsedSinceLastSmear >= smearPeriod)
			{
				LastSmear = cyclestamp;

				// Smear up/down. according to tune.
				if (TuneMicros > 0)
				{
					//ShiftSubSeconds(+1);
				}
				else if (TuneMicros < 0)
				{
					//ShiftSubSeconds(-1);
				}

				return smearPeriod;
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