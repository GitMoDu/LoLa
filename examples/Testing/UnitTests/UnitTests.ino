
#define SERIAL_BAUD_RATE 115200

#define DEBUG_LOLA
#define LOLA_UNIT_TESTING

#define _TASK_OO_CALLBACKS
#define _TASK_SLEEP_ON_IDLE_RUN

#include <TScheduler.hpp>

#include <ILoLaInclude.h>
#include <Arduino.h>
#include "Tests.h"
#include "HopperTest.h"
#include "TestDuplex.h"
#include "TimestampTest.h"
#include "ClockTest.h"

//#include "TestTask.h"

static constexpr uint32_t TestRange = (ONE_SECOND_MICROS / 5) + 1;

// Process scheduler.
TS::Scheduler SchedulerBase;
//

ClockTest ClockTester(SchedulerBase);

void Halt()
{
	Serial.println("[ERROR] Critical Error");
	delay(1000);

	while (1);;
}

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);

	Serial.println(F("LoLa Unit Tests starting."));

	const bool allTestsOk = PerformUnitTests();

	Serial.println();
	Serial.println();
	if (allTestsOk)
	{
		Serial.println(F("LoLa Unit Tests completed with success."));
	}
	else
	{
		Serial.println(F("LoLa Unit Tests FAILED."));
	}
	Serial.println();
}

const bool PerformUnitTests()
{
	bool allTestsOk = true;
	if (HopperTest::RunTests(SchedulerBase))
	{
		Serial.println(F("TestHopper Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestHopper Fail."));
	}

	if (TestDuplex::RunTests())
	{
		Serial.println(F("TestDuplexes Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestDuplexes Fail."));
	}

	if (ClockTester.RunTests<TestRange>())
	{
		Serial.println(F("TestClock Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestClock Fail."));
	}

	if (TimestampTest::RunTests<TestRange>())
	{
		Serial.println(F("TestTimestamp Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestTimestamp Fail."));
	}

	return allTestsOk;
}

void loop()
{
	SchedulerBase.execute();
}

