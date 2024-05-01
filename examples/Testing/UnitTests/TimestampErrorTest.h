// TimestampErrorTest.h

#ifndef _TIMESTAMPERRORTEST_h
#define _TIMESTAMPERRORTEST_h

#include <Arduino.h>
#include "Tests.h"
#include <Clock\Timestamp.h>

class TimestampErrorTest
{
private:
	template<const uint32_t SecondsOffset,
		const uint32_t SubSecondsOffset>
	static const bool TestTimestampError(const int32_t error)
	{
		Timestamp timestamp1{};
		Timestamp timestamp2{};
		TimestampError timestampError{};

		timestamp1.Seconds = SecondsOffset;
		timestamp1.SubSeconds = SubSecondsOffset;
		timestamp1.ConsolidateSubSeconds();

		timestamp2.Seconds = SecondsOffset;
		timestamp2.SubSeconds = SubSecondsOffset;
		timestamp2.ConsolidateSubSeconds();

		timestamp1.ShiftSubSeconds(error);

		timestampError.CalculateError(timestamp1, timestamp2);

		const uint64_t longTimestamp = ((uint64_t)timestamp1.Seconds * ONE_SECOND_MICROS) + timestamp1.SubSeconds;
		const uint64_t longEstimation = ((uint64_t)timestamp2.Seconds * ONE_SECOND_MICROS) + timestamp2.SubSeconds;
		const int64_t errorMicros = longTimestamp - longEstimation;

		const int32_t Seconds = errorMicros / ONE_SECOND_MICROS;
		const int32_t SubSeconds = errorMicros % ONE_SECOND_MICROS;

		const int32_t expectedError = ((int64_t)Seconds * ONE_SECOND_MICROS) + SubSeconds;
		const int32_t calculatedError = (timestampError.Seconds * ONE_SECOND_MICROS) + timestampError.SubSeconds;

		if (expectedError != error
			|| expectedError != calculatedError)
		{
			Serial.println(F("ErrorMicros doesn't match expected."));
			Serial.print('\t');
			timestamp1.print();
			Serial.print('\t');
			timestamp2.print();
			Serial.print('\t');
			timestampError.print();
			Serial.print('\t');
			Serial.print(error);
			Serial.println();

			return false;
		}

		return true;
	}

	template<const uint32_t TestRange,
		const uint32_t SecondsOffset,
		const uint32_t SubSecondsOffset>
	static const bool TestTimestampErrors()
	{
		for (uint32_t i = 0; i < TestRange; i++)
		{
			if (!TestTimestampError<SecondsOffset, SubSecondsOffset>(i))
			{
				return false;
			}
		}
		Serial.print('.');

		for (uint64_t i = (UINT32_MAX / 2) - TestRange; i <= (UINT32_MAX / 2) + TestRange; i++)
		{
			if (!TestTimestampError<SecondsOffset, SubSecondsOffset>(i))
			{
				return false;
			}
		}
		Serial.print('.');

		for (uint64_t i = UINT32_MAX - TestRange; i <= UINT32_MAX; i++)
		{
			if (!TestTimestampError<SecondsOffset, SubSecondsOffset>(i))
			{
				return false;
			}
		}
		Serial.print('.');

		return true;
	}

public:
	template<uint32_t TestRange>
	static const bool RunTests()
	{
		if (!TestTimestampErrors<TestRange, 0, 0>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, 0, SHIFT_LOW>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, 0, SHIFT_MID>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, 0, SHIFT_HIGH>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}

		if (!TestTimestampErrors<TestRange, SHIFT_LOW, 0>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, SHIFT_LOW, SHIFT_LOW>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, SHIFT_LOW, SHIFT_MID>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, SHIFT_LOW, SHIFT_HIGH>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}

		if (!TestTimestampErrors<TestRange, SHIFT_MID, 0>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, SHIFT_MID, SHIFT_LOW>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, SHIFT_MID, SHIFT_MID>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, SHIFT_MID, SHIFT_HIGH>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}

		if (!TestTimestampErrors<TestRange, SHIFT_HIGH, 0>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, SHIFT_HIGH, SHIFT_LOW>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, SHIFT_HIGH, SHIFT_MID>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		if (!TestTimestampErrors<TestRange, SHIFT_HIGH, SHIFT_HIGH>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}
		Serial.println();

		return true;
	}
};
#endif