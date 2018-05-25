// LoLaSi446xPacketDriver.cpp


#include <PacketDriver\LoLaSi446x\LoLaSi446xPacketDriver.h>


//Static handlers for interrupts.
ILoLa* StaticSi446LoLa = nullptr;
volatile uint8_t InterruptStatus = 0;
volatile bool Busy = false;



void SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi)
{
	Busy = true;
	StaticSi446LoLa->OnReceiveBegin(length, rssi);
}

void SI446X_CB_RXINVALID(int16_t rssi)
{
	StaticSi446LoLa->OnReceivedFail(rssi);
}

void SI446X_CB_RXBEGIN(int16_t rssi)
{
	StaticSi446LoLa->OnIncoming(rssi);
}

void SI446X_CB_SENT(void)
{
	StaticSi446LoLa->OnSentOk();
}

void SI446X_CB_WUT(void)
{
	StaticSi446LoLa->OnWakeUpTimer();
}

void SI446X_CB_LOWBATT(void)
{
	StaticSi446LoLa->OnBatteryAlarm();
}

void LoLaSi4463DriverOnReceived(void)
{
	Busy = false;
	StaticSi446LoLa->OnReceived();
#ifndef MOCK_RADIO
	Si446x_irq_on(InterruptStatus);
#endif
}
///////////////////////


LoLaSi446xPacketDriver::LoLaSi446xPacketDriver(Scheduler* scheduler)
	: LoLaPacketDriver()
{

	EventQueueSource = new AsyncActor(scheduler);
	StaticSi446LoLa = this;
}

void LoLaSi446xPacketDriver::OnWakeUpTimer()
{
	LoLaPacketDriver::OnWakeUpTimer();
}

bool LoLaSi446xPacketDriver::Transmit()
{
#ifdef MOCK_RADIO
	delayMicroseconds(500);
	return true;
#else
	if (Sender.GetBufferSize() > 0)
	{
		//On success(has begun transmitting).
		return Si446x_TX(Sender.GetBuffer(), Sender.GetBufferSize(), CurrentChannel, SI446X_STATE_RX) == 1;
	}
	return false;
#endif
}

bool LoLaSi446xPacketDriver::CanTransmit()
{
#ifdef MOCK_RADIO
	return true;
#else
	return !Busy && LoLaPacketDriver::CanTransmit();
#endif		
}

void LoLaSi446xPacketDriver::OnReceiveBegin(const uint8_t length, const int16_t rssi)
{
#ifndef MOCK_RADIO
	Si446x_read(Receiver.GetBuffer(), length);
	Si446x_RX(CurrentChannel);
#endif

	LoLaPacketDriver::OnReceiveBegin(length, rssi);

#ifndef MOCK_RADIO
	//Disable Si interrupts until we have processed the received packet.
	InterruptStatus = Si446x_irq_off();
#endif

	EventQueueSource->AppendEventToQueue(LoLaSi4463DriverOnReceived);
}

void LoLaSi446xPacketDriver::OnStart()
{
#ifndef MOCK_RADIO
	//Si446x_clearFIFO();
	Si446x_RX(CurrentChannel);
#endif
}

bool LoLaSi446xPacketDriver::Setup()
{
	if (LoLaPacketDriver::Setup())
	{
		SetTransmitPower(TRANSMIT_POWER);
		SetChannel(CHANNEL);

#ifndef MOCK_RADIO		
		// Start up
		Si446x_init();
		si446x_info_t info;
		Si446x_getInfo(&info);

		if (info.part == PART_NUMBER_SI4463X)
		{
			Si446x_setTxPower(TransmitPower);
			Si446x_setupCallback(SI446X_CBS_RXBEGIN | SI446X_CBS_SENT, 1); // Enable packet RX begin and packet sent callbacks
			Si446x_setLowBatt(3000); // Set low battery voltage to 3000mV
			Si446x_setupWUT(1, 8192, 0, SI446X_WUT_RUN | SI446X_WUT_BATT); // Run WUT and check battery every 2 seconds

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