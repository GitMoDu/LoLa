// LoLaSi446xSPIDriver.cpp


#include <PacketDriver\RadioDrivers\LoLaSi446xZakkemble\LoLaSi446xPacketDriver.h>

// Static handlers for interrupts.
Si446xWrapperInterface* StaticSi446LoLa = nullptr;



void SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi)
{
	StaticSi446LoLa->OnRadioReceivedOk(length);
}

void SI446X_CB_RXINVALID(int16_t rssi)
{
	StaticSi446LoLa->OnRadioReceiveFail();
}

void SI446X_CB_RXBEGIN(int16_t rssi)
{
	StaticSi446LoLa->OnRadioIncoming(rssi, micros());
}

void SI446X_CB_SENT(void)
{
	StaticSi446LoLa->OnRadioSent();
}

void LoLaSi446xPacketDriver::SetupInterrupts()
{
	StaticSi446LoLa = this;
}
