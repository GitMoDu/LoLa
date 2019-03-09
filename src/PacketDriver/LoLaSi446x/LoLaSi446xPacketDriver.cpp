// LoLaSi446xPacketDriver.cpp


#include <PacketDriver\LoLaSi446x\LoLaSi446xPacketDriver.h>


//Static handlers for interrupts.
LoLaSi446xPacketDriver* StaticSi446LoLa = nullptr;


void SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi)
{
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
LoLaSi446xPacketDriver::LoLaSi446xPacketDriver(Scheduler* scheduler)
	: LoLaPacketDriver(scheduler)
{
	StaticSi446LoLa = this;
}