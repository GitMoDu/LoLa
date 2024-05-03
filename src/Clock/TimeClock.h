// TimeClock.h

#ifndef _TIME_CLOCK_h
#define _TIME_CLOCK_h

#include "CycleClock.h"
#include "Timestamp.h"

/// <summary>
/// Cycle Clock based Time Clock.
/// Provides elapsed seconds and subseconds.
/// Shiftable time.
/// Provides real time based Timestamp with overflows and cycle counts.
/// Rolls over after ~136.1 years.
/// </summary>
class TimeClock : public CycleClock
{
private:
	uint32_t OverflowWrapRemainder = 0;
	uint16_t OverflowWrapSeconds = 0;

private:
	uint32_t OffsetSeconds = 0;
	uint32_t OffsetSubSeconds = 0;

public:
	TimeClock(Scheduler& scheduler, ICycles* cycles)
		: CycleClock(scheduler, cycles)
	{}

	virtual const bool Setup() final
	{
		if (CycleCounter::Setup())
		{
			const uint32_t rolloverPeriod = GetDurationCyclestamp(0, UINT32_MAX);

			OverflowWrapSeconds = rolloverPeriod / ONE_SECOND_MICROS;
			OverflowWrapRemainder = rolloverPeriod % ONE_SECOND_MICROS;

			return true;
		}

		return false;
	}

public:
	const uint32_t GetRollingMicros(const uint32_t cyclestamp)
	{
		const uint32_t overflows = GetCycleOverflows(cyclestamp);
		const uint32_t elapsed = GetElapsedDuration(cyclestamp);
		const uint32_t remainder = overflows * OverflowWrapRemainder;
		const uint32_t seconds = overflows * OverflowWrapSeconds;

		return ((seconds + OffsetSeconds) * ONE_SECOND_MICROS) + remainder + elapsed + OffsetSubSeconds;
	}

	const uint32_t GetRollingMicros()
	{
		const uint32_t cyclestamp = GetCyclestamp();

		return GetRollingMicros(cyclestamp);
	}

	const uint32_t GetRollingMonotonicMicros(const uint32_t cyclestamp)
	{
		const uint32_t overflows = GetCycleOverflows(cyclestamp);
		const uint32_t elapsed = GetElapsedDuration(cyclestamp);
		const uint32_t remainder = overflows * OverflowWrapRemainder;
		const uint32_t seconds = overflows * OverflowWrapSeconds;

		return (seconds * ONE_SECOND_MICROS) + remainder + elapsed;
	}

	const uint32_t GetRollingMonotonicMicros()
	{
		const uint32_t cyclestamp = GetCyclestamp();

		return GetRollingMonotonicMicros(cyclestamp);
	}

	/// <summary>
	/// Non-monotonic, tuned timestamp.
	/// Rolls over after ~136.1 years.
	/// </summary>
	/// <param name="timestamp"></param>
	void GetTimestamp(Timestamp& timestamp)
	{
		const uint32_t cyclestamp = GetCyclestamp();

		GetTimestamp(cyclestamp, timestamp);
	}

	void GetTimestampMonotonic(Timestamp& timestamp)
	{
		const uint32_t cyclestamp = GetCyclestamp();

		GetTimestampMonotonic(cyclestamp, timestamp);
	}

	void GetTimestamp(const uint32_t cyclestamp, Timestamp& timestamp)
	{
		// Start with the monotonic timestamp.
		GetTimestampMonotonic(cyclestamp, timestamp);

		// Shift it with the local offset.
		timestamp.Seconds += OffsetSeconds;
		timestamp.ShiftSubSeconds(OffsetSubSeconds);
	}

	void GetTimestampMonotonic(const uint32_t cyclestamp, Timestamp& timestamp)
	{
		// Get the overflow count.
		const uint32_t overflows = GetCycleOverflows(cyclestamp);
		const uint32_t elapsed = GetElapsedDuration(cyclestamp);

		// Start with the overflows and consolidate.
		timestamp.Seconds = overflows * OverflowWrapSeconds;
		timestamp.SubSeconds = overflows * OverflowWrapRemainder;
		timestamp.ConsolidateSubSeconds();

		// Shift it with the elapsed duration.
		timestamp.ShiftSubSeconds(elapsed);
	}

	void ShiftSeconds(const int32_t offsetSeconds)
	{
		OffsetSeconds += offsetSeconds;
	}

	void ShiftSubSeconds(const int32_t offsetMicros)
	{
		if (offsetMicros > 0)
		{
			if ((OffsetSubSeconds + offsetMicros) < OffsetSubSeconds)
			{
				// Subseconds rollover when adding offset.
				OffsetSeconds += OverflowWrapSeconds;
				OffsetSubSeconds += OverflowWrapRemainder;
			}
		}
		else if (offsetMicros < 0)
		{
			if ((OffsetSubSeconds + offsetMicros) > OffsetSubSeconds)
			{
				// Subseconds rollover when removing offset.
				OffsetSeconds -= OverflowWrapSeconds;
				OffsetSubSeconds -= OverflowWrapRemainder;
			}
		}

		OffsetSubSeconds += offsetMicros;

		if (OffsetSubSeconds >= ONE_SECOND_MICROS)
		{
			OffsetSeconds += OffsetSubSeconds / ONE_SECOND_MICROS;
			OffsetSubSeconds = OffsetSubSeconds % ONE_SECOND_MICROS;
		}
	}
};
#endif