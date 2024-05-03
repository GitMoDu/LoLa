// CycleClock.h

#ifndef _CYCLE_CLOCK_h
#define _CYCLE_CLOCK_h

#include "CycleCounter.h"

/// <summary>
/// Cycle Counter based Cycle Clock.
/// Tracks Cyclestamp overflow count.
/// Uses task callback to ensure cyclestamp' overflows are tracked.
class CycleClock : public CycleCounter
{
private:
	uint32_t LastCyclestamp = 0;
	uint32_t Overflows = 0;

public:
	CycleClock(Scheduler& scheduler, ICycles* cycles)
		: CycleCounter(scheduler, cycles)
	{}

public:
	/// <summary>
	/// Start the clock.
	/// </summary>
	/// <param name="ppm">Starting tune in us.</param>
	virtual void Start()
	{
		LastCyclestamp = 0;
		CycleCounter::Start();
	}

	const uint32_t GetCycleOverflowsEphemeral(const uint32_t cyclestamp)
	{
		if (cyclestamp < LastCyclestamp)
		{
			return Overflows + 1;
		}
		else
		{
			return Overflows;
		}
	}

	const uint32_t GetCycleOverflows(const uint32_t cyclestamp)
	{
		if (cyclestamp < LastCyclestamp)
		{
			Overflows++;
			LastCyclestamp = cyclestamp;
		}
		else
		{
			LastCyclestamp = cyclestamp;
		}

		return Overflows;
	}

protected:
	/// <summary>
	/// Checks for cyclestamp overflows.
	/// Smears the tune offset.
	/// </summary>
	/// <param name="cyclestamp"></param>
	/// <returns>Max delay in milliseconds until next run is required.</returns>
	virtual uint32_t RunCheck(const uint32_t cyclestamp)
	{
		return CheckOverflows(cyclestamp);
	}

private:
	/// <summary>
	/// Check cyclestamp for rollover and compensate in overflows.
	/// </summary>
	/// <param name="cyclestamp"></param>
	/// <returns></returns>
	const bool CheckOverflowsInternal(const uint32_t cyclestamp)
	{
		if (cyclestamp < LastCyclestamp)
		{
			Overflows++;
			LastCyclestamp = cyclestamp;

			return true;
		}
		else
		{
			LastCyclestamp = cyclestamp;

			return false;
		}
	}

	const uint32_t CheckOverflows(const uint32_t cyclestamp)
	{
		if (CheckOverflowsInternal(cyclestamp))
		{
			return 0;
		}
		else
		{
			const uint32_t nextOverflowDelay = GetDurationCyclestamp(cyclestamp, UINT32_MAX);

			if (nextOverflowDelay <= ONE_SECOND_MICROS)
			{
				if (nextOverflowDelay > ONE_MILLI_MICROS)
				{
					// Sleep until we're one milisecond away from overflow.
					return (nextOverflowDelay - ONE_MILLI_MICROS) / ONE_MILLI_MICROS;
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
				return ((nextOverflowDelay - ONE_SECOND_MICROS) / ONE_MILLI_MICROS);
			}
		}
	}
};
#endif