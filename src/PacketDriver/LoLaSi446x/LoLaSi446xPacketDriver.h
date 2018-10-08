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


//Channel to listen to(0 - 26)
#define CHANNEL 0


#define TRANSMIT_POWER 12

//   0 = -32dBm	(<1uW)
//   7 =  0dBm	(1mW)
//  12 =  5dBm	(3.2mW)
//  22 =  10dBm	(10mW)
//  40 =  15dBm	(32mW)
// 100 = 20dBm	(100mW) Requires Dual Antennae
// 127 = ABSOLUTE_MAX
#define SI4463_MAX_TRANSMIT_POWER 40



#define PART_NUMBER_SI4463X 17507


#define SI4463_MIN_RSSI (int16_t(-110))
#define SI4463_MAX_RSSI (int16_t(-50))

#define SI4463_MIN_CHANNEL 5
#define SI4463_MAX_CHANNEL 120


#define _TASK_OO_CALLBACKS
#define PROCESS_EVENT_QUEUE_MAX_QUEUE_DEPTH 5

class LoLaSi446xPacketDriver : public LoLaPacketDriver
{
protected:
	bool Transmit();
	bool CanTransmit();
	void OnStart();

private:
	void DisableInterruptsInternal();

public:
	LoLaSi446xPacketDriver(Scheduler* scheduler);
	bool Setup();
	bool DisableInterrupts();

	void EnableInterrupts();

	void OnReceiveBegin(const uint8_t length, const  int16_t rssi);

	void OnReceivedFail(const int16_t rssi);
	void OnReceived();

	uint8_t GetTransmitPowerMax() const;
	uint8_t GetTransmitPowerMin() const;

	int16_t GetRSSIMax() const;
	int16_t GetRSSIMin() const;

	uint8_t GetChannelMax() const;
	uint8_t GetChannelMin() const;

	void OnChannelUpdated();
	void OnTransmitPowerUpdated();

};
#endif