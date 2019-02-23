// LoLaSi446xPacketDriver.cpp


#include <PacketDriver\LoLaSi446x\LoLaSi446xPacketDriver.h>


//Static handlers for interrupts.
LoLaSi446xPacketDriver* StaticSi446LoLa = nullptr;
volatile bool Receiving = false;

void SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi)
{
	StaticSi446LoLa->OnReceiveBegin(length, rssi);
	Receiving = false;
}

void SI446X_CB_RXINVALID(int16_t rssi)
{
	StaticSi446LoLa->OnReceivedFail(rssi);
	Receiving = false;
}

void SI446X_CB_RXBEGIN(int16_t rssi)
{
	Receiving = true;
	StaticSi446LoLa->OnIncoming(rssi);
}

void SI446X_CB_SENT(void)
{
	StaticSi446LoLa->OnSentOk();
	Receiving = false;
}

void SI446X_CB_WUT(void)
{
	StaticSi446LoLa->OnWakeUpTimer();
}

void SI446X_CB_LOWBATT(void)
{
	StaticSi446LoLa->OnBatteryAlarm();
}

///////////////////////
LoLaSi446xPacketDriver::LoLaSi446xPacketDriver(Scheduler* scheduler)
	: LoLaPacketDriver()
{
	StaticSi446LoLa = this;
}

bool LoLaSi446xPacketDriver::Transmit()
{
#ifdef MOCK_RADIO
	delayMicroseconds(500);
	return true;
#else
	return Sender.GetBufferSize() > 0 && 
		//On success(has begun transmitting).
		Si446x_TX(Sender.GetBuffer(), Sender.GetBufferSize(), CurrentChannel, SI446X_STATE_RX);
#endif
}

bool LoLaSi446xPacketDriver::CanTransmit()
{
#ifdef MOCK_RADIO
	return true;
#else
	return !Receiving;
#endif		
}

void LoLaSi446xPacketDriver::OnReceiveBegin(const uint8_t length, const int16_t rssi)
{
	LoLaPacketDriver::OnReceiveBegin(length, rssi);

#ifndef MOCK_RADIO
	Si446x_read(Receiver.GetBuffer(), Receiver.GetBufferSize());
	//TODO: Process before resetting to RX. Needs separated task for Ack send.
	Si446x_RX(CurrentChannel);
#endif
	OnReceived();
}

void LoLaSi446xPacketDriver::OnChannelUpdated()
{
#ifndef MOCK_RADIO
	Si446x_RX(CurrentChannel);
#endif
}

void LoLaSi446xPacketDriver::OnTransmitPowerUpdated()
{
#ifndef MOCK_RADIO
	Si446x_setTxPower(CurrentTransmitPower);
#endif
}

void LoLaSi446xPacketDriver::OnStart()
{
#ifndef MOCK_RADIO
	Si446x_RX(CurrentChannel);
#endif
}

bool LoLaSi446xPacketDriver::Setup()
{
	if (LoLaPacketDriver::Setup())
	{
#ifndef MOCK_RADIO
		//The SPI interface is designed to operate at a maximum of 10 MHz.
#if defined(ARDUINO_ARCH_AVR)
		SPI.setClockDivider(SPI_CLOCK_DIV2); // 16 MHz / 2 = 8 MHz
#elif defined(ARDUINO_ARCH_STM32F1)
		SPI.setClockDivider(SPI_CLOCK_DIV8); // 72 MHz / 8 = 9 MHz
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
			Serial.println(F("Si4463 Present"));
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
			return false;
		}
#else 
		return true;
#endif
	}

	return false;
}