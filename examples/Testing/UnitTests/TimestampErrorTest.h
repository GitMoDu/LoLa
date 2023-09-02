// TimestampErrorTest.h

#ifndef _TIMESTAMPERRORTEST_h
#define _TIMESTAMPERRORTEST_h

#include <Arduino.h>
#include "Tests.h"

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

		timestamp2.ShiftSubSeconds(error);

		timestampError.CalculateError(timestamp1, timestamp2);

		if (timestampError.ErrorMicros() != error)
		{
			Serial.println(F("ErrorMicros doesn't match expected."));
			Serial.print('\t');
			timestamp1.print();
			Serial.print('\t');
			timestamp2.print();
			Serial.print('\t');
			Serial.print(timestampError.ErrorMicros());
			Serial.print('\t');
			Serial.print(offset);

			return false;
		}

		return true;
	}

	template<const uint32_t Range>
	static const bool TestTimestampErrors(const int32_t error)
	{
		TestTimestampError
	}

public:
	static const bool RunTests()
	{
		if (!TestTimestampErrors<0, 0>())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}

		return true;
	}
};



#endif
