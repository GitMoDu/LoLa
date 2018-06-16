// LoLaSi446xPacketDriver.cpp


#include <PacketDriver\LoLaSi446x\LoLaSi446xPacketDriver.h>


//Static handlers for interrupts.
LoLaSi446xPacketDriver* StaticSi446LoLa = nullptr;
#define UNINITIALIZED_INTERRUPT 0XFF
volatile uint8_t InterruptStatus = UNINITIALIZED_INTERRUPT;
volatile bool Receiving = false;

void SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi)
{
	Receiving = true;
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

///////////////////////

void LoLaSi4463DriverOnReceived(void)
{
	StaticSi446LoLa->OnReceived();
	Receiving = false;
}

void LoLaSi4463DriverCheckForPending(void)
{
	StaticSi446LoLa->CheckForPending();
}

LoLaSi446xPacketDriver::LoLaSi446xPacketDriver(Scheduler* scheduler)
	: LoLaPacketDriver()
	, EventQueue(scheduler)
{
	StaticSi446LoLa = this;
}

void LoLaSi446xPacketDriver::OnWakeUpTimer()
{
	LoLaPacketDriver::OnWakeUpTimer();
	CheckForPendingAsync();
}

void LoLaSi446xPacketDriver::CheckForPending()
{
#ifndef MOCK_RADIO
	Si446x_SERVICE();
#endif
}
bool LoLaSi446xPacketDriver::DisableInterrupts()
{
	if (!Receiving)
	{
#ifndef MOCK_RADIO
		InterruptStatus = Si446x_irq_off();
#endif
		return true;
	}

	return false;
}

void LoLaSi446xPacketDriver::DisableInterruptsInternal()
{
#ifndef MOCK_RADIO
	InterruptStatus = Si446x_irq_off();
#endif
	CheckForPending();
}

void LoLaSi446xPacketDriver::EnableInterrupts()
{
#ifndef MOCK_RADIO
	if (InterruptStatus != UNINITIALIZED_INTERRUPT)
	{
		Si446x_irq_on(InterruptStatus);
	}
	InterruptStatus = UNINITIALIZED_INTERRUPT;
#endif
	CheckForPendingAsync(); //Take this chance to make sure there are no pending interrupts.
}

void LoLaSi446xPacketDriver::CheckForPendingAsync()
{
	//Asynchronously check for pending messages from the radio IC.
	EventQueue.AppendEventToQueue(LoLaSi4463DriverCheckForPending);
}

bool LoLaSi446xPacketDriver::Transmit()
{
#ifdef MOCK_RADIO
	delayMicroseconds(500);
	return true;
#else
	bool Result = false;
	if (Sender.GetBufferSize() > 0)
	{
		//On success(has begun transmitting).
		Result = Si446x_TX(Sender.GetBuffer(), Sender.GetBufferSize(), CurrentChannel, SI446X_STATE_RX);
	}

	CheckForPendingAsync(); //Take this chance to make sure there are no pending interrupts.

	return Result;
#endif
}

bool LoLaSi446xPacketDriver::CanTransmit()
{
#ifdef MOCK_RADIO
	return true;
#else
	return !Receiving && LoLaPacketDriver::CanTransmit();
#endif		
}

void LoLaSi446xPacketDriver::OnReceiveBegin(const uint8_t length, const int16_t rssi)
{
	LoLaPacketDriver::OnReceiveBegin(length, rssi);

	//Disable Si interrupts until we have processed the received packet.
	DisableInterruptsInternal();

	//Asynchronously process the received packet.
	EventQueue.AppendEventToQueue(LoLaSi4463DriverOnReceived);
}

void LoLaSi446xPacketDriver::OnReceived()
{
#ifndef MOCK_RADIO
	Si446x_read(Receiver.GetBuffer(), Receiver.GetBufferSize());
	Si446x_RX(CurrentChannel);
#endif

	LoLaPacketDriver::OnReceived();
	EnableInterrupts();
}

void LoLaSi446xPacketDriver::OnReceivedFail(const int16_t rssi)
{
	LoLaPacketDriver::OnReceivedFail(rssi);
	EnableInterrupts();
}

void LoLaSi446xPacketDriver::OnStart()
{
#ifndef MOCK_RADIO
	InterruptStatus = UNINITIALIZED_INTERRUPT;
	CheckForPending();
	Si446x_RX(CurrentChannel);
#endif
}

uint8_t LoLaSi446xPacketDriver::GetTransmitPowerMax()
{
	return SI4463_MAX_TRANSMIT_POWER;
}
uint8_t LoLaSi446xPacketDriver::GetTransmitPowerMin()
{
	return 0;
}

int16_t LoLaSi446xPacketDriver::GetRSSIMax() 
{ 
	return SI4463_MAX_RSSI;
}
int16_t LoLaSi446xPacketDriver::GetRSSIMin()
{ 
	return SI4463_MIN_RSSI;
}

bool LoLaSi446xPacketDriver::Setup()
{
	if (LoLaPacketDriver::Setup())
	{
		SetTransmitPower(TRANSMIT_POWER);
		SetChannel(CHANNEL);

#ifndef MOCK_RADIO
		//The SPI interface is designed to operate at a maximum of 10 MHz.
#if defined(ARDUINO_ARCH_AVR)
		SPI.setClockDivider(SPI_CLOCK_DIV2); // 16 MHz / 2 = 8 MHz
#elif defined(ARDUINO_ARCH_STM32)

		SPI.setClockDivider(SPI_CLOCK_DIV4); // 72 MHz / 8 = 9 MHz
#endif

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

			CheckForPending();

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