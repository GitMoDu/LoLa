#define SERIAL_BAUD_RATE 115200


#define _TASK_OO_CALLBACKS
#define _TASK_SLEEP_ON_IDLE_RUN

#define LOLA_UNIT_TESTING
#include <TaskScheduler.h>

#include <ILoLaInclude.h>
#include "Tests.h"

Scheduler SchedulerBase;

static constexpr uint32_t CHANNEL_HOP_PERIOD_MICROS = 50000;
static constexpr uint32_t DUPLEX_PERIOD_MICROS = 10000;
static constexpr uint32_t DUPLEX_ASSYMETRIC_RATIO = 10;

NoHopNoChannel ChannelNoHop;
FixedChannelNoHop<1> ChannelFixedNoHop;
NoHopDynamicChannel ChannelDynamicNoHop;
TimedChannelHopper<CHANNEL_HOP_PERIOD_MICROS> ChannelHop(SchedulerBase);


FullDuplex DuplexFull;
HalfDuplex<DUPLEX_PERIOD_MICROS, false, 0> DuplexSymmetricalA;
HalfDuplex<DUPLEX_PERIOD_MICROS, true, 0> DuplexSymmetricalB;
HalfDuplexAsymmetric< DUPLEX_PERIOD_MICROS, UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO, false, 0> DuplexAssymmetricalA;
HalfDuplexAsymmetric< DUPLEX_PERIOD_MICROS, UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO, true, 0> DuplexAssymmetricalB;

SlottedDuplex<DUPLEX_PERIOD_MICROS, 0> DuplexSlottedA;
SlottedDuplex<DUPLEX_PERIOD_MICROS, 0> DuplexSlottedB;
SlottedDuplex<DUPLEX_PERIOD_MICROS, 0> DuplexSlottedC;

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
	if (TestHopperSleepPeriod())
	{
		Serial.println(F("TestHopperSleepPeriod Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestHopperSleepPeriod Fail."));
	}

	if (TestHopperTypes())
	{
		Serial.println(F("TestHopperTypes Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestHopperTypes Fail."));
	}

	if (TestDuplexes())
	{
		Serial.println(F("TestDuplexes Pass."));
	}
	else
	{
		allTestsOk = false;
		Serial.println(F("TestDuplexes Fail."));
	}

	return allTestsOk;
}

void loop()
{
	//SchedulerBase.execute();
}

const bool TestHopperTypes()
{
	if (ChannelNoHop.IsHopper() == true)
	{
		Serial.println(F("NoHopNoChannel claims to be a hopper."));
		return false;
	}

	if (ChannelFixedNoHop.IsHopper() == true)
	{
		Serial.println(F("FixedChannelNoHop claims to be a hopper."));
		return false;
	}

	if (ChannelDynamicNoHop.IsHopper() == true)
	{
		Serial.println(F("NoHopDynamicChannel claims to be a hopper."));
		return false;
	}

	if (ChannelHop.IsHopper() == false)
	{
		Serial.println(F("TimedChannelHopper claims to not be a hopper."));
		return false;
	}

	return true;
}


const bool TestDuplexes()
{
	if (!TestDuplex<true, 0, 0, DUPLEX_PERIOD_MICROS, 0>(&DuplexFull)) { return false; }
	if (!TestDuplex<true, 0, 0, (DUPLEX_PERIOD_MICROS), 100>(&DuplexFull)) { return false; }

	if (!TestDuplex<false, 0, DUPLEX_PERIOD_MICROS / 2, DUPLEX_PERIOD_MICROS, 0>(&DuplexSymmetricalA)) { return false; }
	if (!TestDuplex<false, 0, (DUPLEX_PERIOD_MICROS / 2), DUPLEX_PERIOD_MICROS, 200>(&DuplexSymmetricalA)) { return false; }
	if (!TestDuplex<false, DUPLEX_PERIOD_MICROS / 2, DUPLEX_PERIOD_MICROS, DUPLEX_PERIOD_MICROS, 0>(&DuplexSymmetricalB)) { return false; }
	if (!TestDuplex<false, (DUPLEX_PERIOD_MICROS / 2), DUPLEX_PERIOD_MICROS, DUPLEX_PERIOD_MICROS, 1333>(&DuplexSymmetricalB)) { return false; }


	if (!TestDuplex<false, 0, ((uint32_t)(UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO) * DUPLEX_PERIOD_MICROS) / UINT8_MAX, DUPLEX_PERIOD_MICROS, 0>(&DuplexAssymmetricalA)) { return false; }
	if (!TestDuplex<false, 0, ((uint32_t)(UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO) * DUPLEX_PERIOD_MICROS) / UINT8_MAX, DUPLEX_PERIOD_MICROS, 100>(&DuplexAssymmetricalA)) { return false; }
	if (!TestDuplex<false, ((uint32_t)(UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO) * DUPLEX_PERIOD_MICROS) / UINT8_MAX, DUPLEX_PERIOD_MICROS, DUPLEX_PERIOD_MICROS, 0>(&DuplexAssymmetricalB)) { return false; }
	if (!TestDuplex<false, ((uint32_t)(UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO) * DUPLEX_PERIOD_MICROS) / UINT8_MAX, DUPLEX_PERIOD_MICROS, DUPLEX_PERIOD_MICROS, 100>(&DuplexAssymmetricalB)) { return false; }

	if (!DuplexSlottedA.SetTotalSlots(3)
		|| !DuplexSlottedB.SetTotalSlots(3)
		|| !DuplexSlottedC.SetTotalSlots(3))
	{
		Serial.println(F("SetTotalSlots failed."));
		return false;
	}

	if (!DuplexSlottedA.SetSlot(0)
		|| !DuplexSlottedB.SetSlot(1)
		|| !DuplexSlottedC.SetSlot(2))
	{
		Serial.println(F("SetTotalSlots failed."));
		return false;
	}

	if (!TestDuplex<false, 0, ((uint32_t)1 * DUPLEX_PERIOD_MICROS) / 3, DUPLEX_PERIOD_MICROS, 0 >(&DuplexSlottedA)) { return false; }
	if (!TestDuplex<false, 0, (((uint32_t)1 * DUPLEX_PERIOD_MICROS) / 3), DUPLEX_PERIOD_MICROS, 2555>(&DuplexSlottedA)) { return false; }
	if (!TestDuplex<false, ((uint32_t)1 * DUPLEX_PERIOD_MICROS) / 3, ((uint32_t)2 * DUPLEX_PERIOD_MICROS) / 3, DUPLEX_PERIOD_MICROS, 0>(&DuplexSlottedB)) { return false; }
	if (!TestDuplex<false, (((uint32_t)1 * DUPLEX_PERIOD_MICROS) / 3), ((uint32_t)2 * DUPLEX_PERIOD_MICROS) / 3, DUPLEX_PERIOD_MICROS, 450>(&DuplexSlottedB)) { return false; }
	if (!TestDuplex<false, ((uint32_t)2 * DUPLEX_PERIOD_MICROS) / 3, DUPLEX_PERIOD_MICROS, DUPLEX_PERIOD_MICROS, 0>(&DuplexSlottedC)) { return false; }
	if (!TestDuplex<false, (((uint32_t)2 * DUPLEX_PERIOD_MICROS) / 3), DUPLEX_PERIOD_MICROS, DUPLEX_PERIOD_MICROS, 4202>(&DuplexSlottedC)) { return false; }

	return true;
}

const bool TestHopperSleepPeriod()
{
	if (!TestTimedChannelHopper<500, 0>()) { return false; }
	if (!TestTimedChannelHopper<1000, 0>()) { return false; }
	if (!TestTimedChannelHopper<2000, 1>()) { return false; }
	if (!TestTimedChannelHopper<3000, 2>()) { return false; }
	if (!TestTimedChannelHopper<5000, 4>()) { return false; }
	if (!TestTimedChannelHopper<10000, 9>()) { return false; }
	if (!TestTimedChannelHopper<15000, 14>()) { return false; }
	if (!TestTimedChannelHopper<20000, 18>()) { return false; }
	if (!TestTimedChannelHopper<25000, 23>()) { return false; }
	if (!TestTimedChannelHopper<30000, 28>()) { return false; }
	if (!TestTimedChannelHopper<40000, 37>()) { return false; }
	if (!TestTimedChannelHopper<45000, 42>()) { return false; }
	if (!TestTimedChannelHopper<50000, 47>()) { return false; }
	if (!TestTimedChannelHopper<75000, 70>()) { return false; }
	if (!TestTimedChannelHopper<100000, 94>()) { return false; }

	return true;
}
