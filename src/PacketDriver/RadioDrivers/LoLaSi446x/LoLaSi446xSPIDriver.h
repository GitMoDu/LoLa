// LoLaSi446xSPIDriver.h

#ifndef _LOLASI446XSPIDRIVER_h
#define _LOLASI446XSPIDRIVER_h


#include <stdint.h>
#include <Arduino.h>
#include <SPI.h>
#include <PacketDriver\LoLaPacketDriver.h>
#include <PacketDriver\RadioDrivers\LoLaSi446x\IRadioTask.h>


#define RADIO_USE_DMA
#define USE_RADIO_ATOMIC



class LoLaSi446xSPIDriver : public LoLaPacketDriver
{
private:
#define CHIP_SELECT()	for(uint8_t _cs = On(); _cs; _cs = Off())

#ifdef USE_RADIO_ATOMIC
#define RADIO_ATOMIC() for(uint8_t _cs2 = InterruptOff(); _cs2; _cs2 = InterruptOn())
#else
#define RADIO_ATOMIC() 
#endif

	uint32_t TimeoutHelper = 0;
	IRadioTask* RadioTask = nullptr;

protected:
	static const uint8_t COMMAND_READ_BUFFER = 0x44;


	//Device communication helpers.
	static const uint8_t MESSAGE_OUT_SIZE = 18; //Max internal sent message length;
	static const uint8_t MESSAGE_IN_SIZE = 8; //Max internal received message length;

	uint8_t MessageIn[MESSAGE_IN_SIZE];
	uint8_t MessageOut[MESSAGE_OUT_SIZE];

	static const uint32_t RESPONSE_TIMEOUT_MICROS = 500000;
	static const uint32_t RESPONSE_SHORT_TIMEOUT_MICROS = 250;
	static const uint32_t RESPONSE_CHECK_PERIOD_MICROS = 2;

	//SPI instance.
	SPIClass* spi = nullptr;

	bool InterruptsEnabled = false;
	volatile uint32_t EventTimestamp = 0;

	const uint8_t CS_PIN;
	const uint8_t RESET_PIN;
	const uint8_t IRQ_PIN;

#ifdef DEBUG_LOLA
	uint32_t TimeoutCount = 0;
#endif

public:
	LoLaSi446xSPIDriver(SPIClass* spi_instance, const uint8_t cs, const uint8_t reset, const uint8_t irq);
	//LoLaSi446xSPIDriver(SPIClass* spi_instance, const uint8_t cs, const uint8_t reset, const uint8_t irq)
	//	: LoLaPacketDriver()
	//	, CS_PIN(cs)
	//	, RESET_PIN(reset)
	//	, IRQ_PIN(irq)
	//{
	//	spi = spi_instance;
	//}

	void OnRadioInterrupt()
	{
		EventTimestamp = micros();
		RadioTask->Wake();
	}

protected:
	void AttachInterrupt();

protected:
	virtual void OnResetLiveData()
	{
		LoLaPacketDriver::OnResetLiveData();
		EventTimestamp = 0;
	}

	inline void Wake()
	{
		RadioTask->Wake();
	}

	void SetRadioTask(IRadioTask* radioTask)
	{
		RadioTask = radioTask;
	}

	uint8_t InterruptOff()
	{
		if (InterruptsEnabled)
		{
			InterruptsEnabled = false;
			detachInterrupt(digitalPinToInterrupt(IRQ_PIN));
		}

		return 0;
	}

	uint8_t InterruptOn()
	{
		if (!InterruptsEnabled)
		{
			AttachInterrupt();
			Wake();
			InterruptsEnabled = true;
		}

		return 1;
	}

protected:
	virtual bool RadioSetup()
	{
		if (spi == nullptr || RadioTask == nullptr)
		{
			return false;
		}

		Off();

		pinMode(CS_PIN, OUTPUT);
		pinMode(RESET_PIN, OUTPUT);
		pinMode(IRQ_PIN, INPUT_PULLUP);

		spi->begin();

#define LOLA_SET_SPEED_SPI

#ifdef LOLA_SET_SPEED_SPI
		//The SPI interface is designed to operate at a maximum of 10 MHz.
#if defined(ARDUINO_ARCH_AVR)
		spi->setClockDivider(SPI_CLOCK_DIV2); // 16 MHz / 2 = 8 MHz
#elif defined(ARDUINO_ARCH_STM32F1)
		spi->setClockDivider(SPI_CLOCK_DIV8); // 72 MHz / 8 = 9 MHz
#endif
#endif

		RadioReset();

		return true;
	}


	inline uint8_t On()
	{
		digitalWrite(CS_PIN, LOW);
		return 1;
	}

	inline uint8_t Off()
	{
		digitalWrite(CS_PIN, HIGH);
		return 0;
	}

	// Reset the RF chip
	void RadioReset()
	{
		digitalWrite(RESET_PIN, HIGH);
		delay(20);
		digitalWrite(RESET_PIN, LOW);
		delay(20);
	}

protected:
	// Low level communications.
	bool API(const uint8_t outLength, const uint8_t inLength)
	{
		if (WaitForResponse(MessageIn, 0, RESPONSE_SHORT_TIMEOUT_MICROS)) // Make sure it's ok to send a command
		{
			RADIO_ATOMIC()
			{
				CHIP_SELECT()
				{
#ifdef RADIO_USE_DMA
					spi->dmaSend(MessageOut, inLength);
#else
					for (uint8_t i = 0; i < inLength; i++)
					{
						spi->transfer(MessageOut[i]);
					}
#endif
				}
			}

			if (inLength > 0)
			{
				// If we have an output buffer then read command response into it
				return WaitForResponse(MessageIn, inLength, RESPONSE_SHORT_TIMEOUT_MICROS);
			}
		}

		return false;
	}

	// Keep trying to read the command buffer, with timeout of around 500ms
	bool WaitForResponse(uint8_t* target, const uint8_t inLength, const uint32_t timeoutMicros = RESPONSE_TIMEOUT_MICROS)
	{
		TimeoutHelper = micros() + timeoutMicros;
		while (GetResponse(target, inLength) == false)
		{
			if (micros() < TimeoutHelper)
			{
				delayMicroseconds(RESPONSE_CHECK_PERIOD_MICROS);
			}
			else
			{
#ifdef DEBUG_LOLA
				TimeoutCount++;
#endif
				return false;
			}
		}
		return true;
	}

	// Read CTS and if its ok then read the command buffer
	bool GetResponse(uint8_t* target, const uint8_t inLength)
	{
		bool cts = 0;

		RADIO_ATOMIC()
		{
			CHIP_SELECT()
			{
				// Send command.
				spi->transfer(COMMAND_READ_BUFFER);

				// Get CTS value.
				cts = spi->transfer(0xFF) == 0xFF;

				if (cts)
				{
					// Get response data
#ifdef RADIO_USE_DMA
					//If passed as 0, it sends FF repeatedly for "length" bytes.
					spi->dmaTransfer(0, target, inLength);
#else
					for (uint8_t i = 0; i < inLength; i++)
					{
						target[i] = spi_transfer(0xFF);
					}
#endif					
				}
			}
		}
		return cts;
	}
};
#endif

