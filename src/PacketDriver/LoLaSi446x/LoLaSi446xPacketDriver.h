// LoLaSi446xPacketDriver.h

#ifndef _LOLASI446XPACKETDRIVER_h
#define _LOLASI446XPACKETDRIVER_h

#define _TASK_OO_CALLBACKS

#include <Arduino.h>
#include <TaskSchedulerDeclarations.h>
#include <PacketDriver\LoLaPacketDriver.h>
#include <RingBufCPP.h>

#include <SPI.h>


#ifndef MOCK_RADIO
#include <Si446x.h>
#endif // !MOCK_RADIO


//Channel to listen to(0 - 255)
#define CHANNEL 100


//   0 = -32dBm	(<1uW)
//   7 =  0dBm	(1mW)
//  12 =  5dBm	(3.2mW)
//  22 =  10dBm	(10mW)
//  40 =  15dBm	(32mW)
// 100 = 20dBm	(100mW) Requires Dual Antennae

#define SI4463_MAX_TRANSMIT_POWER 40

#define TRANSMIT_POWER 7

#define PART_NUMBER_SI4463X 17507


 
#define SI4463_MIN_RSSI (int16_t(-150))
#define SI4463_MAX_RSSI (int16_t(-10))


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
	AsyncActor EventQueue;

protected:
	bool Transmit();
	bool CanTransmit();
	void OnStart();

private:
	void CheckForPendingAsync();
	void DisableInterruptsInternal();

public:
	LoLaSi446xPacketDriver(Scheduler* scheduler);
	bool Setup();
	void CheckForPending();
	bool DisableInterrupts();

	void EnableInterrupts();

	void OnWakeUpTimer();
	void OnReceiveBegin(const uint8_t length, const  int16_t rssi);

	void OnReceivedFail(const int16_t rssi);
	void OnReceived();

	uint8_t GetTransmitPowerMax();
	uint8_t GetTransmitPowerMin();

	int16_t GetRSSIMax();
	int16_t GetRSSIMin();

	uint8_t GetRSSI();
};
#endif