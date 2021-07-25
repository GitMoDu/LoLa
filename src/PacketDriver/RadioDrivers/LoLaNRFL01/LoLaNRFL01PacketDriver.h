// LoLaNRFL01PacketDriver.h

#ifndef _LOLA_NRFL01_PACKET_DRIVER_h
#define _LOLA_NRFL01_PACKET_DRIVER_h


#include <SPI.h>
#include <PacketDriver\LoLaPacketDriver>
#include <PacketDriver\RadioDrivers\LoLaNRFL01\LoLaNRFL01RadioDriver.h>


class LoLaNRFL01PacketDriver
	: public LoLaPacketDriver
{
private:
	uint32_t LastEventTimestamp = 0;

	LoLaNRFL01RadioDriver RadioDriver;

	class LoLaNRFL01Settings : public virtual ILoLaPacketDriverSettings
	{
	public:
		LoLaNRFL01Settings() : ILoLaPacketDriverSettings() {}

		virtual const uint8_t GetTransmitPowerMax()
		{
			return 1;
		}

		virtual const uint8_t GetTransmitPowerMin()
		{
			return 0;
		}

		virtual const int16_t GetRSSIMax()
		{
			return -20;
		}

		virtual const int16_t GetRSSIMin()
		{
			return -80;
		}

		virtual const uint8_t GetChannelMax()
		{
			return 10;
		}

		virtual const uint8_t GetChannelMin()
		{
			return 0;
		}

		virtual const uint8_t GetMaxPacketSize()
		{
			return 128;
		}

		virtual const uint32_t GetETTM() { return 1000; }

	} NRFL01Settings;

public:
	LoLaNRFL01PacketDriver(Scheduler* scheduler, SPIClass* spi, const uint8_t cs, const uint8_t reset, const uint8_t irq)
		: LoLaPacketDriver(scheduler, &NRFL01Settings)
		, LoLaNRFL01RadioDriver(spi, cs, reset, irq)
	{
	}

#ifdef DEBUG_LOLA
	virtual void Debug(Stream* serial)
	{
		LoLaPacketDriver::Debug(serial);

		serial->print(F("Driver timeouts: "));
		serial->println(TimeoutCount);
	}
#endif

protected:
	virtual void OnStart()
	{
		SetRadioReceive();
	}

	virtual void OnChannelUpdated()
	{

	}

	virtual void OnTransmitPowerUpdated()
	{

	}

	void SetRadioPower()
	{

	}

	void SetRadioChannel()
	{
	}

	virtual void SetRadioReceive()
	{

	}

	virtual bool RadioSetup()
	{
		byte addresses[][6] = { "1Node","2Node" };

		//Radio.begin();
		//Radio.setPALevel(RF24_PA_LOW);
		///*Radio.openWritingPipe(addresses[1]);
		//Radio.openReadingPipe(1, addresses[0]);*/

		//Radio.startListening();

		//Radio.printDetails();                             // Dump the configuration of the rf unit for debugging
		delay(50);

		return true;
	}
};
#endif