// TimestampTest.h

#ifndef _TIMESTAMPTEST_h
#define _TIMESTAMPTEST_h

#include <Arduino.h>
#include "Tests.h"
#include <Clock\Timestamp.h>

class TimestampTest
{
private:
	static const uint32_t Range = 100000;// ONE_SECOND_MICROS + 1;

private:
	template<const uint32_t SecondsOffset,
		const uint32_t SubSecondsOffset>
	static const bool TestTimestampSecond(const int32_t offset)
	{
		Timestamp timestamp{};

		timestamp.Seconds = SecondsOffset;
		timestamp.SubSeconds = SubSecondsOffset;

		timestamp.ShiftSeconds(offset);
		timestamp.ConsolidateSubSeconds();

		const uint64_t rolling = ((uint64_t)(SecondsOffset + offset) * ONE_SECOND_MICROS) + SubSecondsOffset;

		const uint32_t secondsRolled = rolling / ONE_SECOND_MICROS;
		const uint32_t subSecondsRolled = (rolling % ONE_SECOND_MICROS);

		if (timestamp.Seconds != secondsRolled)
		{
			Serial.println(F("Seconds don't match expected."));
			Serial.print('\t');
			Serial.print(offset);
			Serial.print('\t');
			timestamp.print();
			Serial.print('\t');
			Serial.print(secondsRolled);
			Serial.print('.');
			Serial.println(subSecondsRolled);
			return false;
		}

		const uint32_t timestampRoll = timestamp.GetRollingMicros();

		if (timestampRoll != (rolling & UINT32_MAX))
		{
			Serial.println(F("Rolling seconds don't match expected."));
			Serial.print('\t');
			Serial.print(offset);
			Serial.print('\t');
			timestamp.print();
			Serial.print('\t');
			Serial.print(secondsRolled);
			Serial.print('.');
			Serial.println(subSecondsRolled);

			Serial.print(F("\tRolling: 0x"));
			Serial.println(timestampRoll, HEX);
			Serial.print(F("\tExpected: 0x"));
			Serial.println((rolling & UINT32_MAX), HEX);
			return false;
		}

		return true;
	}

	template<const uint32_t TestRange,
		const uint32_t SecondsOffset,
		const uint32_t SubSecondsOffset>
	static const bool TestTimestampSeconds()
	{
		Serial.print('.');

		for (uint32_t i = 0; i <= TestRange; i++)
		{
			if (!TestTimestampSecond<SecondsOffset, SubSecondsOffset>(i))
			{
				return false;
			}
		}
		Serial.print('.');

		for (uint64_t i = INT32_MAX - TestRange; i <= INT32_MAX + 1 + TestRange; i++)
		{
			if (!TestTimestampSecond<SecondsOffset, SubSecondsOffset>(i))
			{
				return false;
			}
		}
		Serial.print('.');

		for (uint64_t i = UINT32_MAX - TestRange; i <= UINT32_MAX; i++)
		{
			if (!TestTimestampSecond<SecondsOffset, SubSecondsOffset>(i))
			{
				return false;
			}
		}

		return true;
	}

	template<const uint32_t SecondsOffset,
		const uint32_t SubSecondsOffset>
	static const bool TestTimestampSubSecond(const int32_t offset)
	{
		Timestamp timestamp{};

		timestamp.Seconds = SecondsOffset;
		timestamp.SubSeconds = SubSecondsOffset;

		timestamp.ShiftSubSeconds(offset);
		timestamp.ConsolidateSubSeconds();

		const uint64_t rolling = ((uint64_t)(SecondsOffset)*ONE_SECOND_MICROS) + ((uint64_t)(SubSecondsOffset + offset));

		const uint32_t secondsRolled = rolling / ONE_SECOND_MICROS;
		const uint32_t subSecondsRolled = (rolling % ONE_SECOND_MICROS);

		if (timestamp.Seconds != secondsRolled
			|| timestamp.SubSeconds != subSecondsRolled)
		{
			Serial.println(F("Timestamp doesn't match expected."));
			Serial.print('\t');
			Serial.print(offset);
			Serial.print('\t');
			timestamp.print();
			Serial.print('\t');
			Serial.print(secondsRolled);
			Serial.print('.');
			Serial.println(subSecondsRolled);
			return false;
		}


		if (timestamp.GetRollingMicros() != (uint32_t)rolling)
		{
			Serial.println(F("Rolling micros doesn't match expected."));
			Serial.print('\t');
			Serial.print(offset);
			Serial.print('\t');
			timestamp.print();
			Serial.print('\t');
			Serial.print((uint32_t)rolling);
			Serial.println('?');
			Serial.print(timestamp.GetRollingMicros());
			Serial.println('?');
			return false;
		}

		return true;
	}

	template<const uint32_t TestRange,
		const uint32_t SecondsOffset,
		const uint32_t SubSecondsOffset>
	static const bool TestSubSeconds()
	{
		Serial.print('.');
		for (uint32_t i = 0; i < TestRange; i++)
		{
			if (!TestTimestampSubSecond<SecondsOffset, SubSecondsOffset>(i))
			{
				return false;
			}
		}
		Serial.print('.');

		for (uint64_t i = (UINT32_MAX / 2) - TestRange; i <= (UINT32_MAX / 2) + TestRange; i++)
		{
			if (!TestTimestampSubSecond<SecondsOffset, SubSecondsOffset>(i))
			{
				return false;
			}
		}
		Serial.print('.');

		for (uint64_t i = UINT32_MAX - TestRange; i <= UINT32_MAX; i++)
		{
			if (!TestTimestampSubSecond<SecondsOffset, SubSecondsOffset>(i))
			{
				return false;
			}
		}

		return true;
	}

	template<const uint32_t TestRange>
	static const bool TestTimestampShiftSeconds()
	{
		Serial.println(F("Timestamp seconds shift."));
		if (!TestTimestampSeconds<TestRange, 0, 0>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, 0, SHIFT_LOW>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, 0, SHIFT_MID>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, 0, SHIFT_HIGH>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_MID, 0>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_LOW, SHIFT_LOW>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_LOW, SHIFT_MID>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_LOW, SHIFT_HIGH>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_MID, 0>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_MID, SHIFT_LOW>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_MID, SHIFT_MID>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_MID, SHIFT_HIGH>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_HIGH, 0>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_HIGH, SHIFT_LOW>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_HIGH, SHIFT_MID>())
		{
			return false;
		}
		if (!TestTimestampSeconds<TestRange, SHIFT_HIGH, SHIFT_HIGH>())
		{
			return false;
		}

		Serial.println();

		return true;
	}


	template<const uint32_t TestRange>
	static const bool TestTimestampShiftSubSeconds()
	{
		Serial.println(F("Timestamp sub-seconds shift."));
		if (!TestSubSeconds<TestRange, 0, 0>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, 0, SHIFT_LOW>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, 0, SHIFT_MID>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, 0, SHIFT_HIGH>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_LOW, 0>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_LOW, SHIFT_LOW>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_LOW, SHIFT_MID>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_LOW, SHIFT_HIGH>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_MID, 0>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_MID, SHIFT_LOW>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_MID, SHIFT_MID>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_MID, SHIFT_HIGH>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_HIGH, 0>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_HIGH, SHIFT_LOW>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_HIGH, SHIFT_MID>())
		{
			return false;
		}
		if (!TestSubSeconds<TestRange, SHIFT_HIGH, SHIFT_HIGH>())
		{
			return false;
		}

		Serial.println();

		return true;
	}

public:
	template<uint32_t Range>
	static const bool RunTests()
	{
		if (!TestTimestampShiftSeconds<Range>())
		{
			Serial.println(F("TestTimestampShiftSeconds failed"));
			return false;
		}

		if (!TestTimestampShiftSubSeconds<Range>())
		{
			Serial.println(F("TestTimestampShiftSubSeconds failed"));
			return false;
		}

		return true;
	}
};

#endif

