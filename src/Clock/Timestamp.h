// Timestamp.h

#ifndef _TIMESTAMP_h
#define _TIMESTAMP_h

#include "Clock\Time.h"

/// <summary>
/// Timestamp with seconds [0;UINT32_MAX]
/// and subseconds [0;1000000].
/// </summary>
struct Timestamp
{
	uint32_t Seconds = 0;
	uint32_t SubSeconds = 0;

	/// <summary>
	/// </summary>
	/// <param name="offsetSeconds"></param>
	void ShiftSeconds(const int32_t offsetSeconds)
	{
		Seconds += offsetSeconds;
	}

	/// <summary>
	/// Shift SubSeconds by micros and consolidate timestamp.
	/// </summary>
	/// <param name="offsetMicros">In us.</param>
	void ShiftSubSeconds(const int32_t offsetMicros)
	{
		SubSeconds += offsetMicros;

		ConsolidateSubSeconds();
	}

	/// <summary>
	/// Consolidate subseconds into seconds.
	/// </summary>
	void ConsolidateSubSeconds()
	{
		if (SubSeconds >= ONE_SECOND_MICROS)
		{
			Seconds += SubSeconds / ONE_SECOND_MICROS;
			SubSeconds %= ONE_SECOND_MICROS;
		}
	}

	const uint32_t GetRollingMicros()
	{
		return (Seconds * ONE_SECOND_MICROS) + SubSeconds;
	}
	
#if defined(DEBUG_LOLA) || defined(DEBUG_LOLA_LINK)
	void print()
	{
		Serial.print(Seconds);
		Serial.print('.');
		if (SubSeconds < 100000) Serial.print(0);
		if (SubSeconds < 10000) Serial.print(0);
		if (SubSeconds < 1000) Serial.print(0);
		if (SubSeconds < 100) Serial.print(0);
		if (SubSeconds < 10) Serial.print(0);
		Serial.print(SubSeconds);
	}
#endif

	static const int32_t GetDeltaSeconds(const Timestamp& timestampStart, const Timestamp& timestampEnd)
	{
		int32_t seconds;
		int32_t subSeconds;
		if (timestampEnd.Seconds >= timestampStart.Seconds)
		{
			seconds = timestampEnd.Seconds - timestampStart.Seconds - 1;
			subSeconds = ONE_SECOND_MICROS;
		}
		else
		{
			seconds = UINT32_MAX - timestampStart.Seconds + timestampEnd.Seconds - 1;
			subSeconds = -(int32_t)ONE_SECOND_MICROS;
		}

		if (timestampEnd.SubSeconds >= timestampStart.SubSeconds)
		{
			subSeconds += timestampEnd.SubSeconds - timestampStart.SubSeconds;
		}
		else
		{
			subSeconds += UINT32_MAX - timestampStart.SubSeconds + timestampEnd.SubSeconds;
		}

		seconds += subSeconds / (int32_t)ONE_SECOND_MICROS;

		return seconds;
	}
};
#endif