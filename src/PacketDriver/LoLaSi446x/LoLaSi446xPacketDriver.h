// LoLaSi446xPacketDriver.h

#ifndef _LOLASI446XPACKETDRIVER_h
#define _LOLASI446XPACKETDRIVER_h

#include <Arduino.h>
#include <SPI.h>
#include <PacketDriver\LoLaPacketDriver.h>
#include <RingBufCPP.h>

#ifndef MOCK_RADIO
#include <Si446x.h>
#endif // !MOCK_RADIO

//Expected part number.
#define PART_NUMBER_SI4463X 17507

//Channel to listen to (0 - 26).
#define SI4463_CHANNEL_MIN 0
#define SI4463_CHANNEL_MAX 26

//Power range.
//   0 = -32dBm	(<1uW)
//   7 =  0dBm	(1mW)
//  12 =  5dBm	(3.2mW)
//  22 =  10dBm	(10mW)
//  40 =  15dBm	(32mW)
// 100 = 20dBm	(100mW) Requires Dual Antennae
// 127 = ABSOLUTE_MAX
#define SI4463_TRANSMIT_POWER_MIN	0
#define SI4463_TRANSMIT_POWER_MAX	40

//Received RSSI range.
#define SI4463_RSSI_MIN (int16_t(-110))
#define SI4463_RSSI_MAX (int16_t(-50))

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

	void OnReceiveBegin(const uint8_t length, const int16_t rssi);

	void OnChannelUpdated();
	void OnTransmitPowerUpdated();

	///Driver constants.
	uint8_t GetTransmitPowerMax() const
	{
		return SI4463_TRANSMIT_POWER_MAX;
	}

	uint8_t GetTransmitPowerMin() const
	{
		return SI4463_TRANSMIT_POWER_MIN;
	}

	int16_t GetRSSIMax() const
	{
		return SI4463_RSSI_MAX;
	}

	int16_t GetRSSIMin() const
	{
		return SI4463_RSSI_MIN;
	}

	uint8_t GetChannelMax() const
	{
		return SI4463_CHANNEL_MAX;
	}

	uint8_t GetChannelMin() const
	{
		return SI4463_CHANNEL_MIN;
	}
	///
};
#endif