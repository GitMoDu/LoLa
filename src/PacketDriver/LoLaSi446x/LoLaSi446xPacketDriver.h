// LoLaSi446xPacketDriver.h

#ifndef _LOLASI446XPACKETDRIVER_h
#define _LOLASI446XPACKETDRIVER_h

#include <Arduino.h>
#include <SPI.h>
#include <PacketDriver\LoLaPacketDriver.h>
#include <RingBufCPP.h>

#ifndef LOLA_MOCK_RADIO
#include <Si446x.h>
#endif

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
	//Not working properly.
	void DisableInterrupts()
	{
#ifndef LOLA_MOCK_RADIO
		//Si446x_irq_off();
#endif
	}

	//Not working properly.
	void EnableInterrupts()
	{
#ifndef LOLA_MOCK_RADIO
		//Si446x_irq_on(0xFF);
#endif
	}

	void SetRadioPower()
	{
#ifndef LOLA_MOCK_RADIO
		Si446x_setTxPower(CurrentTransmitPower);
#endif
	}

	bool Transmit()
	{
#ifdef LOLA_MOCK_RADIO
		delayMicroseconds(500);
		return true;
#else
		return Si446x_TX(OutgoingPacket.GetRaw(), OutgoingPacketSize, CurrentChannel, SI446X_STATE_RX);
#endif
	}

	void ReadReceived()
	{
#ifndef LOLA_MOCK_RADIO
		Si446x_read(IncomingPacket.GetRaw(), IncomingPacketSize);
#endif
	}

	void SetToReceiving()
	{
#ifndef LOLA_MOCK_RADIO
		Si446x_RX(CurrentChannel);
#endif
	}

	bool SetupRadio()
	{
#ifdef LOLA_MOCK_RADIO
		return true;
#else
		//The SPI interface is designed to operate at a maximum of 10 MHz.
#if defined(ARDUINO_ARCH_AVR)
		SPI.setClockDivider(SPI_CLOCK_DIV2); // 16 MHz / 2 = 8 MHz
#elif defined(ARDUINO_ARCH_STM32F1)
#ifdef SI446x_DRIVER_OVERCLOCK
		SPI.setClockDivider(SPI_CLOCK_DIV4); // 72 MHz / 4 = 18 MHz
#else
		SPI.setClockDivider(SPI_CLOCK_DIV8); // 72 MHz / 8 = 9 MHz
#endif
#endif

		// Start up
		Si446x_init();
		si446x_info_t info;
		Si446x_getInfo(&info);

		if (info.part == PART_NUMBER_SI4463X)
		{
			Si446x_setTxPower(CurrentTransmitPower);
			Si446x_setupCallback(SI446X_CBS_RXBEGIN | SI446X_CBS_SENT, 1); // Enable packet RX begin and packet sent callbacks
			Si446x_setLowBatt(3200); // Set low battery voltage to 3200mV
			Si446x_setupWUT(1, 8192, 0, SI446X_WUT_BATT); // Run check battery every 2 seconds.

			Si446x_SERVICE();
			Si446x_sleep();
#ifdef DEBUG_LOLA
			//Serial.println(F("Si4463 Present"));
			//Serial.println(info.revBranch);
			//Serial.println(info.revExternal);
			//Serial.println(info.revInternal);
			//Serial.println(info.chipRev);
			//Serial.println(info.customer);
			//Serial.println(info.id);
			//Serial.println(info.patch);
			//Serial.println(info.partBuild);
			//Serial.println(info.func);
			//Serial.println(info.romId);
#endif // DEBUG_LOLA
			return true;
		}
		else
		{
#ifdef DEBUG_LOLA
			Serial.print(F("Part number invalid: "));
			Serial.println(info.part);
			Serial.println(F("Si4463 Driver failed to start."));
#endif // DEBUG_LOLA
		}

		return false;
#endif
	}

public:
	LoLaSi446xPacketDriver(Scheduler* scheduler);

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