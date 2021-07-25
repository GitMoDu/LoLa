// LoLaSi446xSPIDriver.cpp


#include <PacketDriver\RadioDrivers\LoLaNRFL01\LoLaNRFL01SPIDriver.h>


//Static handlers for interrupts.
static LoLaNRFL01SPIDriver* StaticLoLa = nullptr;


static void NRFL01Interrupt()
{
	StaticLoLa->OnRadioInterrupt(micros());
}

/////////////////////////
LoLaNRFL01SPIDriver::LoLaNRFL01SPIDriver(SPIClass* spi_instance, const uint8_t cs, const uint8_t ce, const uint8_t irq)
	//, Radio(ce, cs)
	: CS_PIN(cs)
	, CE_PIN(ce)
	, IRQ_PIN(irq)
{
	spi = spi_instance;
	StaticLoLa = this;
}

void LoLaNRFL01SPIDriver::AttachInterrupt()
{
	attachInterrupt(digitalPinToInterrupt(IRQ_PIN), NRFL01Interrupt, FALLING);
}