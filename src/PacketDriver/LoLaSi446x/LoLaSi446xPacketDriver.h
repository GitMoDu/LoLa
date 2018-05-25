// LoLaSi446xPacketDriver.h

#ifndef _LOLASI446XPACKETDRIVER_h
#define _LOLASI446XPACKETDRIVER_h

#define _TASK_OO_CALLBACKS

#include <TaskSchedulerDeclarations.h>
#include <PacketDriver\LoLaPacketDriver.h>
#include <RingBufCPP.h>


#ifndef MOCK_RADIO
#include <Si446x.h>
#endif // !MOCK_RADIO



//Channel to listen to(0 - 255)
#define CHANNEL 100


//*0 = -32dBm(<1uW)\n
//* 7 = 0dBm(1mW)\n
//* 12 = 5dBm(3.2mW)\n
//* 22 = 10dBm(10mW)\n
//* 40 = 15dBm(32mW)\n
//* 100 = 20dBm(100mW)
#define TRANSMIT_POWER 7

#define PART_NUMBER_SI4463X 17507

#define _TASK_OO_CALLBACKS
#define PROCESS_EVENT_QUEUE_MAX_QUEUE_DEPTH 5

class AsyncActor : Task
{
private:
	RingBufCPP<void(*)(void), PROCESS_EVENT_QUEUE_MAX_QUEUE_DEPTH> EventQueue;

	void(*GruntFunction)(void) = nullptr;

public:
	AsyncActor(Scheduler* scheduler)
		: Task(0, TASK_FOREVER, scheduler, false)
	{
	}

	void AppendEventToQueue(void(*callFunction)(void))
	{
		if (callFunction != nullptr)
		{
			EventQueue.addForce(callFunction);
		}

		if (!EventQueue.isEmpty())
		{
			enable();
		}
	}


	bool OnEnable()
	{
		if (EventQueue.isEmpty())
		{
			disable();
		}

		return true;
	}

	void OnDisable()
	{
	}

	bool Callback()
	{
		EventQueue.pull(GruntFunction);

		if (GruntFunction != nullptr)
		{
			GruntFunction();
		}

		if (EventQueue.isEmpty())
		{
			disable();
		}

		return false;
	}
};

class LoLaSi446xPacketDriver : public LoLaPacketDriver
{
private:
	AsyncActor* EventQueueSource;

protected:
	bool Transmit();

	bool CanTransmit();
	void OnStart();


public:
	LoLaSi446xPacketDriver(Scheduler* scheduler);
	bool Setup();
	void OnWakeUpTimer();
	void OnReceiveBegin(const uint8_t length, const  int16_t rssi);

};
#endif