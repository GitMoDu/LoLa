// TimeClock.h

#ifndef _TIME_CLOCK_h
#define _TIME_CLOCK_h

#include "CycleClock.h"
#include "Timestamp.h"

/// <summary>
/// Cycle Clock based Time Clock.
/// Provides elapsed seconds and subseconds.
/// Shiftable time.
/// Provides real time based Timestamp.
/// Rolls over after ~136.1 years.
/// </summary>
class TimeClock : public CycleClock
{
private:
	static constexpr uint32_t OVER_SECONDS = 4154500390;// INT32_MAX% ONE_SECOND_MICROS;
	static constexpr uint32_t OVER_SUB_SECONDS = 584320;// INT32_MAX / ONE_SECOND_MICROS;
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

	void GetTimestamp(const uint32_t cyclestamp, Timestamp& timestamp)
	{
		// Get the overflow count.
		const uint32_t overflows = GetCycleOverflows(cyclestamp);

		// Start with duration from cycle clock and consolidate.
		// This enableds the full range of UINT32_MAX us.
		timestamp.Seconds = 0;
		timestamp.SubSeconds += GetElapsedDuration(cyclestamp);
		timestamp.ConsolidateOverflow();

		// Shift cycle clock overflows.
		timestamp.ShiftSeconds(overflows * OverflowWrapSeconds);
		timestamp.ShiftSubSeconds(overflows * OverflowWrapRemainder);

		// Shift the local offset.
		timestamp.ShiftSeconds(OffsetSeconds);
		timestamp.ShiftSubSeconds(OffsetSubSeconds);
	}

	void ShiftSeconds(const int32_t offsetSeconds)
	{
		OffsetSeconds += offsetSeconds;
	}

	void ShiftSubSeconds(const int32_t offsetMicros)
	{
		if (offsetMicros > 0
			&& ((OffsetSubSeconds + offsetMicros) < OffsetSubSeconds))
		{
			// Subseconds rollover when removing offset.
			OffsetSeconds -= OVER_SECONDS;
			OffsetSubSeconds += offsetMicros - OVER_SUB_SECONDS;
		}
		else
		{
			OffsetSubSeconds += offsetMicros;
		}

		if (OffsetSubSeconds >= ONE_SECOND_MICROS)
		{
			OffsetSeconds += OffsetSubSeconds / ONE_SECOND_MICROS;
			OffsetSubSeconds %= ONE_SECOND_MICROS;
		}
	}
};
#endif