// LoLaSi446xSPIDriver.cpp


#include <PacketDriver\RadioDrivers\LoLaSi446x\LoLaSi446xSPIDriver.h>
#include <SPI.h>

//Static handlers for interrupts.
LoLaSi446xSPIDriver* StaticSi446LoLa = nullptr;


void SI446xInterrupt()
{
	StaticSi446LoLa->OnRadioInterrupt();
}

/////////////////////////
LoLaSi446xSPIDriver::LoLaSi446xSPIDriver(SPIClass* spi_instance, const uint8_t cs, const uint8_t reset, const uint8_t irq)
	: LoLaPacketDriver()
	, CS_PIN(cs)
	, RESET_PIN(reset)
	, IRQ_PIN(irq)
{
	spi = spi_instance;
	StaticSi446LoLa = this;
}

void LoLaSi446xSPIDriver::AttachInterrupt()
{
	attachInterrupt(digitalPinToInterrupt(IRQ_PIN), SI446xInterrupt, FALLING);
}