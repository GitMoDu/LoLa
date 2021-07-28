#define WAIT_FOR_LOGGER

#define SERIAL_BAUD_RATE 115200


#define _TASK_OO_CALLBACKS

#include <TaskScheduler.h>
#include <LoLaDriverPIM.h>


#include <LoLaLink\LoLaLinkHost.h>
//#include "ReceptionTester.h"

// Process scheduler.
Scheduler SchedulerBase;
//

const uint8_t MaxPacketSize = 8;

// Radio driver.
LoLaPIMPacketDriver<MaxPacketSize, 2, 7> PIMDriver(&SchedulerBase);

// LoLaLink
LoLaLinkHost<MaxPacketSize, 2, 2> LoLaLink(&SchedulerBase, &PIMDriver);
//ReceptionTester Tester(&SchedulerBase, &LoLaLink);

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


	/*if (!Tester.Setup())
	{
		Serial.println(F("Tester Setup Failed."));
		Halt();
	}*/

	if (!LoLaLink.Setup())
	{
		Serial.println(F("LoLaLink Setup Failed."));
		Halt();
	}


	/*LoLaDriver.Debug(&Serial);

	Tester.Enable();*/

	Serial.println();
	Serial.println(F("LoLa Reception Test Start."));
	Serial.println();
}

void loop()
{
	SchedulerBase.execute();
}