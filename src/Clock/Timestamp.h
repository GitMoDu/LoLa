// Timestamp.h
#ifndef _TIMESTAMP_h
#define _TIMESTAMP_h

#include <stdint.h>
#include <ITimerSource.h>



static constexpr uint32_t ONE_SECOND_MICROS = 1000000;
static constexpr uint32_t ONE_SECOND_MILLIS = 1000;

static constexpr uint32_t ONE_MILLI_MICROS = 1000;


struct Timestamp
{
private:
	/// <summary>
	/// UINT32_MAX/1000000 us	= 4294.967295 s
	/// UINT32_MAX/1000000		= 4294
	/// </summary>
	static constexpr uint32_t SECONDS_ROLLOVER = UINT32_MAX / ONE_SECOND_MICROS;

	/// <summary>
	/// UINT32_MAX/1000000 us	= 4294.967295 s
	/// UINT32_MAX%1000000		= 967295
	/// </summary>
	static constexpr uint32_t SUB_SECONDS_ROLLOVER = UINT32_MAX % ONE_SECOND_MICROS;

public:
	uint32_t Seconds = 0;
	uint32_t SubSeconds = 0;

	/// <summary>
	/// Validate timestamp, SubSeconds should never exceed one second.
	/// </summary>
	/// <returns>True if valid.</returns>
	const bool Validate()
	{
		return SubSeconds < ONE_SECOND_MICROS;
	}

	/// Shift SubSeconds by micros and consolidate timestamp.
	void ShiftSubSeconds(const int32_t offsetMicros)
	{
		if (offsetMicros > 0)
		{
			if ((SubSeconds + offsetMicros) < SubSeconds)
			{
				// Subseconds rollover when adding offset.
				Seconds += SECONDS_ROLLOVER;
				SubSeconds += offsetMicros + SUB_SECONDS_ROLLOVER;
			}
			else
			{
				SubSeconds += offsetMicros;
			}
			ConsolidateOverflow();
		}
		else if (offsetMicros < 0)
		{
			if ((SubSeconds + offsetMicros) > SubSeconds)
			{
				// Subseconds rollover when removing offset.
				Seconds -= SECONDS_ROLLOVER;
				SubSeconds += offsetMicros - SUB_SECONDS_ROLLOVER;
			}
			else
			{
				SubSeconds += offsetMicros;
			}
			ConsolidateOverflow();
		}
	}

	/// Push overflow of subseconds into seconds.
	void ConsolidateOverflow()
	{
		if (SubSeconds > ONE_SECOND_MICROS)
		{
			Seconds += SubSeconds / ONE_SECOND_MICROS;
			SubSeconds %= ONE_SECOND_MICROS;
		}
	}

	const uint32_t GetRollingMicros()
	{
		return (Seconds * ONE_SECOND_MICROS) + SubSeconds;
	}
};

struct TimestampError
{
	int32_t Seconds;
	int32_t SubSeconds;

	/// <summary>
	/// Invalidate estimate error, sub-seconds should never exceed one second.
	/// </summary>
	/// <returns>True if valid.</returns>
	const bool Validate()
	{
		if (SubSeconds >= 0)
		{
			return SubSeconds < (int32_t)ONE_SECOND_MICROS;
		}
		else
		{
			return SubSeconds > -((int32_t)ONE_SECOND_MICROS);
		}
	}

	const int32_t ErrorMicros()
	{
		return (((int64_t)Seconds * ONE_SECOND_MICROS)) + SubSeconds;
	}

	void CalculateError(const Timestamp& timestamp, const Timestamp& estimation)
	{
		Seconds = timestamp.Seconds - estimation.Seconds;
		SubSeconds = timestamp.SubSeconds - estimation.SubSeconds;

		// Consolidate SubSeconds.
		while (SubSeconds < 0)
		{
			SubSeconds += ONE_SECOND_MICROS;
			Seconds--;
		}
		while (SubSeconds > (int32_t)ONE_SECOND_MICROS)
		{
			SubSeconds -= ONE_SECOND_MICROS;
			Seconds++;
		}
	}
};
#endif