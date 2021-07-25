/**
* Low Latency Example Host
*
* LoLa depends on:
* Radio Driver for Si446x: https://github.com/zkemble/Si446x
* Ringbuffer: https://github.com/wizard97/Embedded_RingBuf_CPP
* Generic Callbacks: https://github.com/tomstewart89/Callback
* OO TaskScheduler: https://github.com/arkhipenko/TaskScheduler
* FastCRC with added STM32F1 support: GitMoDu/FastCRC https://github.com/GitMoDu/FastCRC
*	forked from FrankBoesing/FastCRC https://github.com/FrankBoesing/FastCRC
* Memory efficient tracker: https://github.com/GitMoDu/BitTracker.git
* Micro Elyptic Curve Cryptography: https://github.com/kmackay/micro-ecc
* Crypto Library for Ascon128: https://github.com/rweather/arduinolibs
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



#include <Callback.h>
#include <LoLaDriverSi446x.h>
//#include <LoLaDriverNRFL01.h>
#include "HostManager.h"

#include "ClockTestTaskClass.h"


///Process scheduler.
Scheduler SchedulerBase;
///

///SPI Master.
SPIClass SPIWire(1);
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

//LoLaPacketDriverSettings MockDriverSettings(1000, -20, -80, 10, 0, 127, 1);
//LoLaAsyncPacketDriver LoLaDriver(&SchedulerBase, &MockDriverSettings);
//LoLaNRFL01PacketDriver LoLaDriver(&SchedulerBase, &SPIWire, PA4, PB0, PA3);
LoLaSi446xPacketDriver LoLaDriver(&SchedulerBase);
HostManager LoLaManager(&SchedulerBase, &LoLaDriver);
///

///Communicated Data
//ExampleControllerSurface* ControllerInput = nullptr;
///


//ClockTestTaskClass TestClockTask(&SchedulerBase, &LoLaDriver);

void Halt()
{
	Serial.println("Critical Error");
	delay(1000);

	while (1);;
}


void OnLinkStatusUpdated(const bool linked)
{
}

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);
	Serial.println(F("Example Host"));

	if (!LoLaManager.Setup())
	{
		Halt();
	}
	//
	//	FunctionSlot<const bool> funcSlot(OnLinkStatusUpdated);
	//	LoLaDriver.LinkInfo.AttachOnLinkStatusUpdated(funcSlot);
	//
	//	if (LoLaManager.GetControllerSurface() == nullptr)
	//	{
	//		Halt();
	//	}
	//	ControllerInput = LoLaManager.GetControllerSurface();
	//
	//	FunctionSlot<const bool> ptrSlot(OnSurfaceUpdated);
	//	ControllerInput->AttachOnSurfaceUpdated(ptrSlot);
	//

#if defined(DEBUG_LOLA)
	Serial.print(F("LoLa Radio Setup with protocol version: "));
	Serial.println(LoLaDriver.LinkInfo.LinkProtocolVersion);
	LoLaDriver.Debug(&Serial);
#endif

	LoLaManager.Start();
	Serial.println();
}

void OnSurfaceUpdated(const bool dataGood)
{
#if defined(DEBUG_LOG)
	if (dataGood)
		ControllerInput->Debug(&Serial);
#endif
}

void loop()
{
	SchedulerBase.execute();
}