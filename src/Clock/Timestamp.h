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
	/// Validate timestamp, SubSeconds should never exceed or match one second.
	/// </summary>
	/// <returns>True if valid.</returns>
	const bool Validate()
	{
		return SubSeconds < ONE_SECOND_MICROS;
	}

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
};

/// <summary>
/// Timestamp differential error.
/// </summary>
struct TimestampError
{
	int32_t Seconds;
	int32_t SubSeconds;

#if defined(DEBUG_LOLA) || defined(DEBUG_LOLA_LINK)
	void print()
	{
		Serial.print(Seconds);
		Serial.print('.');
		if (SubSeconds < 0)
		{
			Serial.print('-');
		}
		const uint32_t subSeconds = abs(SubSeconds);
		if (subSeconds < 100000) Serial.print(0);
		if (subSeconds < 10000) Serial.print(0);
		if (subSeconds < 1000) Serial.print(0);
		if (subSeconds < 100) Serial.print(0);
		if (subSeconds < 10) Serial.print(0);
		Serial.print(abs(SubSeconds));
		Serial.print('\t');
	}
#endif

	/// <summary>
	/// Invalidate estimate error, sub-seconds should never exceed one second.
	/// </summary>
	/// <returns>True if valid.</returns>
	const bool Validate()
	{
		return SubSeconds < (int32_t)ONE_SECOND_MICROS && SubSeconds > -((int32_t)ONE_SECOND_MICROS);
	}

	/// <summary>
	/// Fill error with delta from estimation to timestamp.
	/// </summary>
	/// <param name="timestamp"></param>
	/// <param name="estimation"></param>
	void CalculateError(const Timestamp& timestamp, const Timestamp& estimation)
	{
		Seconds = timestamp.Seconds - estimation.Seconds;
		SubSeconds = timestamp.SubSeconds - estimation.SubSeconds;

		// Consolidate SubSeconds.
		if (SubSeconds >= (int32_t)ONE_SECOND_MICROS
			|| SubSeconds <= -(int32_t)ONE_SECOND_MICROS)
		{
			Seconds += SubSeconds / (int32_t)ONE_SECOND_MICROS;
			SubSeconds %= (int32_t)ONE_SECOND_MICROS;
		}
	}
};
#endif