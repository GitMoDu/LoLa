// LoLaNRFL01SPIDriver.h

#ifndef _LOLA_NRFL01_SPI_DRIVER_h
#define _LOLA_NRFL01_SPI_DRIVER_h


#include <stdint.h>
#include <Arduino.h>
#include <SPI.h>
#include <LoLaDefinitions.h>
#include <PacketDriver\LoLaPacketDriver.h>
//#include <nRF24L01-STM.h>
//#include <RF24-STM.h>
//#include <printf.h""




#define RADIO_CHIP_SELECT()	for(uint8_t _cs = On(); _cs; _cs = Off())
#define RADIO_ATOMIC()	for(uint8_t _cs2 = InterruptOff(); _cs2; _cs2 = InterruptOn())

class LoLaNRFL01SPIDriver
{
private:
	

protected:
	//RF24 Radio;

	//SPI instance.
	SPIClass* spi = nullptr;

	bool InterruptsEnabled = false;
	volatile uint32_t EventTimestamp = 0;

	const uint8_t CS_PIN;
	const uint8_t CE_PIN;
	const uint8_t IRQ_PIN;

#ifdef DEBUG_LOLA
	uint32_t TimeoutCount = 0;
#endif

public:
	// Declared on LoLaSi446xSPIDriver.cpp
	LoLaNRFL01SPIDriver(SPIClass* spi_instance, const uint8_t cs, const uint8_t ce, const uint8_t irq);

	void OnRadioInterrupt(const uint32_t timestamp)
	{
		EventTimestamp = timestamp;
		//RadioTask->Wake();
	}

protected:
	// Declared on LoLaNRFL01SPIDriver.cpp
	void AttachInterrupt();

protected:
	void Wake()
	{
		//RadioTask->Wake();
	}

	uint8_t InterruptOff()
	{
		if (InterruptsEnabled)
		{
			InterruptsEnabled = false;
			detachInterrupt(digitalPinToInterrupt(IRQ_PIN));
		}

		return 1;
	}

	uint8_t InterruptOn()
	{
		if (!InterruptsEnabled)
		{
			AttachInterrupt();
			Wake();
			InterruptsEnabled = true;
		}

		return 0;
	}

protected:
	virtual bool RadioSetup()
	{

		if (spi == nullptr)
		{
			return false;
		}

		Off();

		pinMode(CS_PIN, OUTPUT);
		pinMode(CE_PIN, OUTPUT);
		pinMode(IRQ_PIN, INPUT_PULLUP);

		spi->begin();

		//RadioReset();

		return true;
	}

	uint8_t On()
	{
		digitalWrite(CS_PIN, LOW);
		return 1;
	}

	uint8_t Off()
	{
		digitalWrite(CS_PIN, HIGH);
		return 0;
	}
};

#endif