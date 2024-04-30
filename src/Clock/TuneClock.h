// TuneClock.h

#ifndef _TUNE_CLOCK_h
#define _TUNE_CLOCK_h

#include "TimeClock.h"

/// <summary>
/// Tuneable timestamp clock.
/// Tracks Cyclestamp overflow count.
/// Uses task callback to smear the tune offset, 
///  as well as ensure cyclestamp' overflows are tracked.
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

			if (ppm > LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE)
			{
				TuneMicros = LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
			}
			else if (ppm < -LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE)
			{
				TuneMicros = -LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
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

	const int16_t GetTune() const
	{
		return TuneMicros;
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
				if ((int32_t)TuneMicros + offsetMicros < LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE)
				{
					TuneMicros += offsetMicros;
				}
				else
				{
					TuneMicros = LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
				}
			}
			else if (offsetMicros < 0)
			{
				if ((int32_t)TuneMicros + offsetMicros > -LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE)
				{
					TuneMicros += offsetMicros;
				}
				else
				{
					TuneMicros = -LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
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
	const uint32_t GetAdaptiveSmearPeriod() const
	{
		if (TuneMicros >= LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE)
		{
			return ONE_SECOND_MILLIS / LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
		}
		else if (TuneMicros <= -LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE)
		{
			return ONE_SECOND_MILLIS / LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
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
					ShiftSubSeconds(+1);
				}
				else if (TuneMicros < 0)
				{
					ShiftSubSeconds(-1);
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