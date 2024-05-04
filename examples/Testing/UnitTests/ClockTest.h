// ClockTest.h

#ifndef _CLOCKTEST_h
#define _CLOCKTEST_h

#include <Arduino.h>
#include "Tests.h"
#include <Clock\LinkClock.h>


class ClockTest
{
private:
	struct MockCyclesSource : public ICycles
	{
		uint32_t Cycles = 0;
		uint32_t Overflows = 0;

		MockCyclesSource()
			: ICycles()
		{}

		const uint32_t GetCycles() final
		{
			return Cycles;
		}

		const uint32_t GetCyclesOverflow() final
		{
			return UINT32_MAX;
		}

		const uint32_t GetCyclesOneSecond() final
		{
			return ONE_SECOND_MICROS;
		}

		void StartCycles() final { }
		void StopCycles() final {}
	} CyclesSource{};

private:
	LinkClock Clock;

public:
	ClockTest(Scheduler& scheduler)
		: Clock(scheduler, &CyclesSource)
	{}


public:
	const bool RunTests()
	{
		if (!TestClockMatching())
		{
			Serial.println(F("TestClockMatching failed"));
			return false;
		}


		return true;
	}

	const bool TestClockMatching()
	{
		for (uint32_t i = 0; i < 1000000; i++)
		{
			if (!TestClockPoint(i, (i * 123) % ONE_SECOND_MICROS))
			{
				return false;
			}

			if (!TestClockPoint(i + INT32_MAX - 100000, (i * 321) % ONE_SECOND_MICROS))
			{
				return false;
			}
		}

		return true;
	}


private:
	const bool TestClockPoint(const uint32_t seconds, const uint32_t subSeconds)
	{
		const uint64_t sourceMicros = ((uint64_t)seconds * ONE_SECOND_MICROS) + subSeconds;

		Timestamp timestamp{};
		Timestamp timestampFromShift{};

		CyclesSource.Overflows = 0;
		CyclesSource.Cycles = 0;

		Clock.SetOffset(seconds, subSeconds);
		Clock.GetTimestamp(timestamp);

		Clock.SetOffset(0, 0);
		if (seconds > INT32_MAX)
		{
			Clock.ShiftSeconds(-(int32_t)(UINT32_MAX - seconds + 1));
		}
		else
		{
			Clock.ShiftSeconds(seconds);
		}
		if (subSeconds > INT32_MAX)
		{
			Clock.ShiftSubSeconds(-(int32_t)(UINT32_MAX - subSeconds));
		}
		else
		{
			Clock.ShiftSubSeconds(subSeconds);
		}
		Clock.GetTimestamp(timestampFromShift);

		if (timestamp.Seconds != timestampFromShift.Seconds)
		{
			Serial.print(F("Clock Shift Seconds failed at\t"));
			Serial.print(seconds);
			Serial.print('.');
			Serial.println(subSeconds);

			Serial.print(F("tTimestamp\t"));
			Serial.print(timestamp.Seconds);
			Serial.print(F("\tTimestampShift\t"));
			Serial.println(timestampFromShift.Seconds);
			return false;
		}

		if (timestamp.SubSeconds != timestampFromShift.SubSeconds)
		{
			Serial.print(F("Clock Shift SubSeconds failed at\t"));
			Serial.print(seconds);
			Serial.print('.');
			Serial.println(subSeconds);

			Serial.print(F("tTimestamp\t"));
			Serial.print(timestamp.SubSeconds);
			Serial.print(F("\tTimestampShift\t"));
			Serial.println(timestampFromShift.SubSeconds);
			return false;
		}

		if (timestamp.Seconds != seconds)
		{
			Serial.print(F("Clock Seconds failed at\t"));
			Serial.print(seconds);
			Serial.print('.');
			Serial.println(subSeconds);

			Serial.print(F("Source\t"));
			Serial.print(seconds);
			Serial.print(F("\tTimestamp\t"));
			Serial.println(timestamp.Seconds);
			return false;
		}

		if (timestamp.GetRollingMicros() != Clock.GetRollingMicros())
		{
			Serial.print(F("Clock GetRollingMicros failed at\t"));
			Serial.print(seconds);
			Serial.print('.');
			Serial.println(subSeconds);
			Serial.print(F("Clock\t"));
			Serial.print(Clock.GetRollingMicros());
			Serial.print(F("\tTimestamp\t"));
			Serial.println(timestamp.GetRollingMicros());
			return false;
		}

		if (timestamp.GetRollingMicros() != (uint32_t)sourceMicros)
		{
			Serial.print(F("Clock Match RollingMicros failed at\t"));
			Serial.print(seconds);
			Serial.print('.');
			Serial.println(subSeconds);
			Serial.print(F("Source\t"));
			Serial.print((uint32_t)sourceMicros);
			Serial.print(F("\tTimestamp\t"));
			Serial.println(timestamp.GetRollingMicros());
			return false;
		}

		return true;
	}

};

#endif