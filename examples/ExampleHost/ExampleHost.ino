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


#include <TaskScheduler.h>

#include <Callback.h>
#include <LoLaDriverSi446x.h>
#include "HostManager.h"


///Process scheduler.
Scheduler SchedulerBase;
///

///Radio manager and driver.
LoLaSi446xPacketDriver LoLaDriver(&SchedulerBase);
HostManager LoLa(&SchedulerBase, &LoLaDriver);
///

///Communicated Data
ControllerSurface * ControllerInput = nullptr;
///


void Halt()
{
#ifdef DEBUG_LOG
	Serial.println("Critical Error");
	delay(1000);
#endif	
	while (1);;
}


void OnLinkStatusUpdated(const LoLaLinkInfo::LinkStateEnum state)
{
#ifdef DEBUG_LOG
	switch (state)
	{
	case LoLaLinkInfo::LinkStateEnum::Setup:
		Serial.println(F("Link Setup"));
		break;
	case LoLaLinkInfo::LinkStateEnum::AwaitingLink:
		Serial.println(F("Broadcasting"));
		break;
	case LoLaLinkInfo::LinkStateEnum::AwaitingSleeping:
		Serial.println(F("Sleeping"));
		break;
	case LoLaLinkInfo::LinkStateEnum::Linking:
		//Serial.println(F("Linking"));
		break;
	case LoLaLinkInfo::LinkStateEnum::Linked:
		Serial.println(F("Linked"));
		break;
	case LoLaLinkInfo::LinkStateEnum::Disabled:
		Serial.println(F("Disabled"));
		break;
	default:
		Serial.print(F("Conn what? "));
		Serial.println(state);
		break;
	}
#endif
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


	if (!LoLa.Setup())
	{
		Halt();
	}

	FunctionSlot<const LoLaLinkInfo::LinkStateEnum> funcSlot(OnLinkStatusUpdated);
	LoLa.GetLinkInfo()->AttachOnLinkStatusUpdated(funcSlot);

	if (LoLa.GetControllerSurface() == nullptr)
	{
		Halt();
	}
	ControllerInput = LoLa.GetControllerSurface();

	FunctionSlot<uint8_t> ptrSlot(OnSurfaceUpdated);
	ControllerInput->AttachOnSurfaceUpdated(ptrSlot);

#if defined(DEBUG_LOG) && defined(DEBUG_LOLA)
	LoLaDriver.Debug(&Serial);
#endif

	LoLa.Start();

#if defined(DEBUG_LOG) && defined(DEBUG_LOLA)
	ControllerInput->NotifyDataChanged();
#endif
}

void OnSurfaceUpdated(uint8_t param)
{
#if defined(DEBUG_LOG) && defined(DEBUG_LOLA)
	ControllerInput->Debug(&Serial);
#endif
}

void loop()
{
	SchedulerBase.execute();
}