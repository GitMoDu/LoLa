/**
* Low Latency Example Host
*
* LoLa depends on:
* Radio Driver for Si446x: https://github.com/zkemble/Si446x
* Ringbuffer: https://github.com/wizard97/Embedded_RingBuf_CPP
* Generic Callbacks: https://github.com/tomstewart89/Callback
* OO TaskScheduler: https://github.com/arkhipenko/TaskScheduler
* Memory efficient tracker: https://github.com/GitMoDu/BitTracker.git
*
*/


#define DEBUG_LOG

#ifdef DEBUG_LOG
#define SERIAL_BAUD_RATE 57600
#endif


#define _TASK_OO_CALLBACKS
#define _TASK_PRIORITY          // Support for layered scheduling priority


#include <TaskScheduler.h>

#include <Callback.h>
#include <LoLaDriver.h>
#include "HostManager.h"


///Process scheduler.
Scheduler SchedulerBase, SchedulerHighPriority;
///

///Radio manager and driver.
LoLaSi446xPacketDriver LoLaDriver(&SchedulerHighPriority);
HostManager LoLa(&SchedulerBase, &LoLaDriver);
///

///Communicated Data
CommandInputSurface * CommandInput = nullptr;
///


void Halt()
{
#ifdef DEBUG_LOG
	Serial.println("Critical Error");
	delay(1000);
#endif	
	while (1);;
}

void setup()
{
#ifdef DEBUG_LOG
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;
	delay(1000);
	Serial.println(F("Example Host"));
#endif	

	SchedulerBase.setHighPriorityScheduler(&SchedulerHighPriority);

	if (!LoLa.Setup())
	{
		Halt();
	}

	if (LoLa.GetCommandInputSurface() == nullptr)
	{
		Halt();
	}
	CommandInput = LoLa.GetCommandInputSurface();

	FunctionSlot<uint8_t> ptrSlot(OnSurfaceUpdated);
	CommandInput->AttachOnSurfaceUpdated(ptrSlot);

#if defined(DEBUG_LOG) && defined(DEBUG_LOLA)
	LoLaDriver.Debug(&Serial);
#endif

	if (!LoLa.Start())
	{
#ifdef DEBUG_LOG
		Serial.println(F("LoLa Startup failed."));
#endif
		Halt();
	}

#if defined(DEBUG_LOG) && defined(DEBUG_LOLA)
	CommandInput->NotifyDataChanged();
#endif
}

void OnSurfaceUpdated(uint8_t param)
{
#if defined(DEBUG_LOG) && defined(DEBUG_LOLA)
	CommandInput->Debug(&Serial);
#endif
}

void loop()
{
	SchedulerBase.execute();
}