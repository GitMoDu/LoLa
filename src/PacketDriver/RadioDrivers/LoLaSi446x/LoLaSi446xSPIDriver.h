// LoLaSi446xSPIDriver.h

#ifndef _LOLASI446XSPIDRIVER_h
#define _LOLASI446XSPIDRIVER_h


#include <stdint.h>
#include <Arduino.h>
#include <SPI.h>
#include <LoLaDefinitions.h>
#include <PacketDriver\LoLaPacketDriver.h>
#include <PacketDriver\RadioDrivers\LoLaSi446x\IRadioTask.h>
#include <PacketDriver\RadioDrivers\LoLaSi446x\Si446x.h>




class LoLaSi446xSPIDriver : public LoLaPacketDriver
{
private:
	uint32_t TimeoutHelper = 0;
	IRadioTask* RadioTask = nullptr;

protected:
#define CHIP_SELECT()	for(uint8_t _cs = On(); _cs; _cs = Off())
#define RADIO_ATOMIC()	for(uint8_t _cs2 = InterruptOff(); _cs2; _cs2 = InterruptOn())

	//Device communication helpers.
	static const uint8_t MESSAGE_OUT_SIZE = 18; //Max internal sent message length;
	static const uint8_t MESSAGE_IN_SIZE = 9; //Max internal received message length;

	uint8_t MessageIn[MESSAGE_IN_SIZE];
	uint8_t MessageOut[MESSAGE_OUT_SIZE];

	static const uint32_t RESPONSE_LONG_TIMEOUT_MICROS = 2000;
	static const uint32_t RESPONSE_SHORT_TIMEOUT_MICROS = 250;
	static const uint32_t RESPONSE_WAIT_TIMEOUT_MICROS = 500;
	static const uint32_t RESPONSE_CHECK_PERIOD_MICROS = 10;
	static const uint32_t RESPONSE_DMA_BACKOFF_PERIOD_MICROS = 20;

	static const uint32_t RADIO_RESET_PERIOD_MICROS = 20000;

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
	// Declared on LoLaSi446xSPIDriver.cpp
	LoLaSi446xSPIDriver(SPIClass* spi_instance, const uint8_t cs, const uint8_t reset, const uint8_t irq);

	void OnRadioInterrupt(const uint32_t timestamp)
	{
		EventTimestamp = timestamp;
		RadioTask->Wake();
	}

protected:
	// Declared on LoLaSi446xSPIDriver.cpp
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
#if defined(ARDUINO_ARCH_STM32F1)
		spi->setClockDivider(SPI_CLOCK_DIV8); // 72 MHz / 8 = 9 MHz
#endif
#endif

		RadioReset();

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

	// Reset the RF chip
	void RadioReset()
	{
		digitalWrite(RESET_PIN, HIGH);
		delayMicroseconds(RADIO_RESET_PERIOD_MICROS);
		digitalWrite(RESET_PIN, LOW);
		delayMicroseconds(RADIO_RESET_PERIOD_MICROS);
	}

protected:
	// Low level communications.
	bool API(const uint8_t outLength, const uint8_t inLength, const uint32_t timeoutMicros = RESPONSE_LONG_TIMEOUT_MICROS)
	{
		if (!WaitForResponse(MessageIn, 0, RESPONSE_WAIT_TIMEOUT_MICROS)) // Make sure it's ok to send a command
		{
			return false;
		}

		RADIO_ATOMIC()
		{
			CHIP_SELECT()
			{
				for (uint8_t i = 0; i < outLength; i++)
				{
					spi->transfer(MessageOut[i]);
				}
			}
		}

		if (inLength == 0)
		{
			return true;
		}
		else
		{
			// If we have an output buffer then read command response into it
			return WaitForResponse(MessageIn, inLength, timeoutMicros);
		}
	}

	// Keep trying to read the command buffer, with timeout of around 500ms
	bool WaitForResponse(uint8_t* target, const uint8_t inLength, const uint32_t timeoutMicros)
	{
		TimeoutHelper = micros() + timeoutMicros;
		uint32_t Start = TimeoutHelper - timeoutMicros;
		uint32_t Tries = 1;
		while (GetResponse(target, inLength) == false)
		{
			if (micros() < TimeoutHelper)
			{
				Tries++;
				delayMicroseconds(RESPONSE_CHECK_PERIOD_MICROS);
			}
			else
			{
				return false;
			}
		}

		//#ifdef DEBUG_RADIO_DRIVER
		//		if (inLength > 0)
		//		{
		//			Serial.print(F("WaitForResponse: OK: "));
		//			Serial.print(micros() - Start);
		//			Serial.print(F(" us Tries: "));
		//			Serial.println(Tries);
		//		}
		//#endif

		return true;
	}

	// Read CTS and if its ok then read the command buffer
	bool GetResponse(uint8_t* target, const uint8_t inLength)
	{
		bool cts = false;

		RADIO_ATOMIC()
		{
			CHIP_SELECT()
			{
				// Send command.
				spi->transfer(Si446x::COMMAND_READ_BUFFER);

				// Get CTS value.
				cts = spi->transfer(0xFF) == 0xFF;

				if (cts)
				{
					// Get response data
					for (uint8_t i = 0; i < inLength; i++)
					{
						target[i] = spi->transfer(0xFF);
					}
				}
			}
		}

		return cts;
	}
};
#endif