// LoLaNRFL01RadioDriver.h

#ifndef _LOLA_NRFL01_RADIO_DRIVER_h
#define _LOLA_NRFL01_RADIO_DRIVER_h


#include <PacketDriver\RadioDrivers\LoLaNRFL01\LoLaNRFL01SPIDriver.h>

class LoLaNRFL01RadioDriver : public LoLaNRFL01SPIDriver
{
private:
public:
	LoLaNRFL01RadioDriver(SPIClass* spi_instance, const uint8_t cs, const uint8_t reset, const uint8_t irq)
		: LoLaNRFL01SPIDriver(spi_instance, cs, reset, irq)
	{
	}

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		serial->print(F("LoLaNRFL01RadioDriver"));
		serial->println();
	}
#endif


protected:
	virtual bool RadioSetup()
	{
		if (!LoLaNRFL01SPIDriver::RadioSetup())
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("LoLaSi446xSPIDriver setup failed."));
#endif
		}


		// Start up.
		InterruptsEnabled = false;
		InterruptOn();

		return true;
	}
};

#endif

