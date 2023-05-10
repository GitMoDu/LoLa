#define WAIT_FOR_LOGGER

#define SERIAL_BAUD_RATE 115200


#define _TASK_OO_CALLBACKS

#include <TaskScheduler.h>
#include <LoLaDriverPIM.h>



#include <LoLaLinkLight\LoLaLinkLight.h>
//#include <LoLaLink\LoLaLinkBase.h>
//#include "ReceptionTester.h"

// Process scheduler.
Scheduler SchedulerBase;
//

const uint8_t MaxPacketSize = 32;

// Radio driver.
LoLaPIMPacketDriver<MaxPacketSize> PIMDriver(&SchedulerBase, 2, 7);

// LoLaLink
LoLaLinkLight<MaxPacketSize> LoLaLink(&SchedulerBase, &PIMDriver);
//ReceptionTester Tester(&SchedulerBase, &LoLaLink);

class PacketListener : public virtual ILoLaLinkPacketListener
{
public:
	PacketListener() : ILoLaLinkPacketListener()
	{

	}

public:
	// Return true if driver should return an ack.
	virtual const bool OnPacketReceived(uint8_t* data, const uint32_t startTimestamp, const uint8_t header, const uint8_t packetSize)
	{ 
		return false;
	}

	virtual void OnPacketAckReceived(const uint32_t startTimestamp, const uint8_t header) 
	{ 
	}


};

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