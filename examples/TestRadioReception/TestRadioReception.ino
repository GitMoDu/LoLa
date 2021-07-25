#define WAIT_FOR_LOGGER

#define SERIAL_BAUD_RATE 250000


#define _TASK_OO_CALLBACKS
#define _TASK_SLEEP_ON_IDLE_RUN

#include <TaskScheduler.h>
#include <LoLaDriverSi446x.h>
#include <SPI.h>
#include "ReceptionTester.h"

///Process scheduler.
Scheduler SchedulerBase;
///

///SPI Master.
SPIClass SPIWire(1);
///

///Radio manager and driver.
LoLaSi446xPacketTaskDriver LoLaDriver(&SchedulerBase, &SPIWire, PA4, PB0, PA3);

ReceptionTester Tester(&SchedulerBase, &LoLaDriver);

void Halt()
{
	Serial.println("[ERROR] Critical Error");
	delay(1000);

	while (1);;
}

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
#ifdef WAIT_FOR_LOGGER
	while (!Serial)
		;
#else
	delay(1000);
#endif
	delay(1000);
	Serial.println(F("LoLa Reception Test Setup"));


	if (!Tester.Setup()) 
	{
		Serial.println(F("Tester Setup Failed."));
		Halt();
	}

	if (!LoLaDriver.Setup())
	{
		Serial.println(F("LoLaDriver Setup Failed."));
		Halt();
	}


	LoLaDriver.Debug(&Serial);

	Tester.Enable();

	Serial.println();
	Serial.println(F("LoLa Reception Test Start."));
	Serial.println();
}

void loop()
{
	SchedulerBase.execute();
}