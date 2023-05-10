/**
* LoLa Example Host
*
* LoLa depends on:
* [DEPRECATED]Radio Driver for Si446x: https://github.com/zkemble/Si446x
* OO TaskScheduler: https://github.com/arkhipenko/TaskScheduler
* FrankBoesing/FastCRC https://github.com/FrankBoesing/FastCRC
* Micro Elyptic Curve Cryptography: https://github.com/kmackay/micro-ecc
* Crypto Library for Ascon128: https://github.com/rweather/arduinolibs
* Memory efficient tracker: https://github.com/GitMoDu/BitTracker.git
*
*/


#define DEBUG_LOLA


#ifdef DEBUG_LOLA
#define DEBUG_TRACKED_SURFACE
#endif

#define SERIAL_BAUD_RATE 115200


#define _TASK_OO_CALLBACKS
#define _TASK_SLEEP_ON_IDLE_RUN
#include <TaskScheduler.h>



#include <LoLaPacketDriversInclude.h>
//#include <LoLaDriverNRFL01.h>
#include <LoLaLinkInclude.h>


//#include "ClockTestTaskClass.h"


///Process scheduler.
Scheduler SchedulerBase;
///

///SPI Master.
//SPI SPIWire(1);
///

///Radio manager and driver.
//TODO:
// Pinout:
// IRQ - (PA3 / D8).
// SDN - (PB0 / D3) //TODO: Use ShutDown (SDN) pin for power management.
// CS - (PA4 / D7) Default SPI1.
// CLK - (PA5 / D6) Default SPI1.
// MISO - (PA6 / D5) Default SPI1.
// MOSI - (PA7 / D4) Default SPI1.

// Link Constants.
static const uint32_t DuplexPeriod = 4;
static const uint32_t MaxPayloadSize = 32;

//LoLaAsyncPacketDriver LoLaDriver(&SchedulerBase, &MockDriverSettings);
//LoLaNRFL01PacketDriver LoLaDriver(&SchedulerBase, &SPIWire, PA4, PB0, PA3);
//LoLaSi446xPacketDriver LoLaDriver(&SchedulerBase);
SerialLoLaPacketDriver<MaxPayloadSize, HardwareSerial, 115200> SerialDriver(SchedulerBase, &Serial2);
PIMLoLaPacketDriver<MaxPayloadSize> PIMDriver(SchedulerBase, 2, 3, 1, 0);

ArduinoSynchronizedClock HostClock;
HalfDuplex<DuplexPeriod, true> HostDuplex;
ITokenSource HopToken;

LoLaLinkHost<MaxPayloadSize> LinkHost(SchedulerBase, &PIMDriver, &HostClock, &HostDuplex, &HopToken);
///


//ClockTestTaskClass TestClockTask(&SchedulerBase, &LoLaDriver);

void Halt()
{
	Serial.println("Critical Error");
	delay(1000);


	while (1);;
}

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);
	Serial.println(F("Example Host"));

	if (!LinkHost.Setup())
	{
		Halt();
	}

	if (LinkHost.Start())
	{
#ifdef DEBUG
		SerialOut.println(F("LoLa Link Virtual Tester Start!"));
#endif
	}
	else
	{
#ifdef DEBUG
		SerialOut.println(F("Link Start Failed."));
#endif
	}

	//#if defined(DEBUG_LOLA)
	//	Serial.print(F("LoLa Radio Setup with protocol version: "));
	//	Serial.println(LoLaDriver.LinkInfo.LinkProtocolVersion);
	//	LoLaDriver.Debug(&Serial);
	//#endif
	Serial.println();
}

void loop()
{
	SchedulerBase.execute();
}


void serialEvent()
{
	SerialDriver.OnSerialEvent();
}