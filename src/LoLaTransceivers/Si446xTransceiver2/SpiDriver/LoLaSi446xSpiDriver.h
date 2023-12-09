// LoLaSi446xSpiDriver.h

#ifndef _LOLA_SI446X_SPI_DRIVER_h
#define _LOLA_SI446X_SPI_DRIVER_h

#include "LoLaSi446x.h"

#include <SPI.h>
#include <Arduino.h>

/// <summary>
/// </summary>
/// <typeparam name="CsPin"></typeparam>
template<const uint8_t CsPin>
class LoLaSi446xSpiDriver
{
private:
	using RadioStateEnum = LoLaSi446x::RadioStateEnum;

private:
	//SPI instance.
	SPIClass* SpiInstance;

	//Device communication helpers.
	static constexpr uint8_t MESSAGE_OUT_SIZE = 18; //Max internal sent message length;
	static constexpr uint8_t MESSAGE_IN_SIZE = 8; //Max internal received message length;

	uint8_t MessageIn[MESSAGE_IN_SIZE]{};
	uint8_t MessageOut[MESSAGE_OUT_SIZE]{};

public:
	LoLaSi446xSpiDriver(SPIClass* spiInstance)
		: SpiInstance(spiInstance)
	{
		pinMode(CsPin, INPUT);
	}

public:

	virtual void Stop()
	{
		if (SpiInstance != nullptr)
		{
			// TODO: Send shutdown command
		}

		// Disable IO pins.
		CsOff();
		pinMode(CsPin, INPUT);
	}

	virtual const bool Start()
	{
		if (SpiInstance == nullptr)
		{
			return false;
		}

		// Setup IO pins.
		CsOff();
		pinMode(CsPin, OUTPUT);

		// Start SPI hardware.
		SpiInstance->begin();
		SpiInstance->setBitOrder(MSBFIRST);
		SpiInstance->setDataMode(SPI_MODE0);

		//The SPI interface is designed to operate at a maximum of 10 MHz.
		// TODO: Adjust according to F_CPU
#if defined(ARDUINO_ARCH_AVR)
		// 16 MHz / 2 = 8 MHz
		SpiInstance->setClockDivider(SPI_CLOCK_DIV2);
#elif defined(ARDUINO_ARCH_STM32F1)
		// 72 MHz / 8 = 9 MHz
		SpiInstance->setClockDivider(SPI_CLOCK_DIV8);
#elif defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
		// TODO:
#else
		// Default SPI speed.
#endif

		return true;
	}

public:
	const bool SpinWaitClearToSend(const uint16_t timeoutMillis)
	{
		const uint32_t start = millis();
		while (!ClearToSend())
		{
			if (millis() - start > timeoutMillis)
			{
				return false;
			}
			else
			{
				delayMicroseconds(5);
			}
		}

		return true;
	}

	const bool SetRadioState(const RadioStateEnum newState)
	{
		MessageOut[0] = LoLaSi446x::COMMAND_SET_STATUS;
		MessageOut[1] = (const uint8_t)newState;

		return SendRequest(MessageOut, 2);
	}

	RadioStateEnum GetRadioState()
	{
		if (!ClearToSend())
		{
			return RadioStateEnum::NO_CHANGE;
		}

		const RadioStateEnum radioState = (RadioStateEnum)GetFrr(LoLaSi446x::COMMAND_READ_FRR_B);
		Serial.print(F("Internal Radio State:"));
		Serial.println((uint8_t)radioState);

		return radioState;
		return (RadioStateEnum)GetFrr(LoLaSi446x::COMMAND_READ_FRR_B);
	}

	/// <summary>
	/// Read pending interrupts.
	/// Reading interrupts will also clear them.
	/// </summary>
	/// <param name="radioEvents"></param>
	/// <returns></returns>
	const bool GetRadioEvents(LoLaSi446x::RadioEventStruct& radioEvents)
	{
		if (!ClearToSend())
		{
			return false;
		}

		if (!SendRequest(LoLaSi446x::COMMAND_GET_INTERRUPT_STATUS))
		{
			return false;
		}

		if (!TryGetResponse(MessageIn, 8))
		{
			return false;
		}

		radioEvents.RxStart = MessageIn[4] & LoLaSi446x::PENDING_EVENT_SYNC_DETECT;
		radioEvents.RxReady = MessageIn[2] & LoLaSi446x::PENDING_EVENT_RX;
		radioEvents.RxFail = MessageIn[2] & LoLaSi446x::PENDING_EVENT_CRC_ERROR;
		radioEvents.TxDone = MessageIn[2] & LoLaSi446x::PENDING_EVENT_SENT_OK;
		radioEvents.WakeUp = MessageIn[2] & LoLaSi446x::PENDING_EVENT_WUT;

		return true;
	}

	/// <summary>
	/// Read events to clear.
	/// </summary>
	/// <returns></returns>
	const bool ClearRadioEvents(const uint16_t timeoutMillis)
	{
		if (!SpinWaitClearToSend(timeoutMillis))
		{
			return false;
		}

		if (!SendRequest(LoLaSi446x::COMMAND_GET_INTERRUPT_STATUS))
		{
			return false;
		}

		const uint32_t start = millis();
		while (!ClearToSend())
		{
			if (millis() - start > timeoutMillis)
			{
				return false;
			}
			else
			{
				delayMicroseconds(5);
			}
		}

		if (!TryGetResponse(MessageIn, 8))
		{
			return false;
		}

		return true;
	}

	const bool RadioStartRx(const uint8_t channel)
	{
		// Initiate Rx.
		MessageOut[0] = LoLaSi446x::COMMAND_START_RX;
		MessageOut[1] = channel;
		MessageOut[2] = 0;
		MessageOut[3] = 0;
		MessageOut[4] = 0;
		MessageOut[5] = (uint8_t)RadioStateEnum::NO_CHANGE;
		MessageOut[6] = (uint8_t)RadioStateEnum::READY;
		MessageOut[7] = (uint8_t)RadioStateEnum::SLEEP;

		return SendRequest(MessageOut, 8);
	}

	const bool RadioStartTx(const uint8_t channel)
	{
		// Initiate Tx.
		MessageOut[0] = LoLaSi446x::COMMAND_START_TX;
		MessageOut[1] = channel;
		MessageOut[2] = (uint8_t)RadioStateEnum::RX << 4; // On transmitted restore state.
		MessageOut[3] = 0;
		MessageOut[4] = 0;
		MessageOut[5] = 0;
		MessageOut[6] = 0;

		//if (!SpinWaitClearToSend(50))
		//{
		//	return false;
		//}

		return SendRequest(MessageOut, 7);
	}


	const bool SetTxFifo(const uint8_t* source, const uint8_t size)
	{
		bool cts = 0;

		CsOn();

		// Send read command and check if available.
		SpiInstance->transfer(LoLaSi446x::COMMAND_WRITE_TX_FIFO);
		cts = SpiInstance->transfer(0xFF) == 0xFF;

		if (cts)
		{
			SpiInstance->transfer((uint8_t*)source, size);
		}
		CsOff();

		return cts;
	}

	const bool GetRxFifo(uint8_t* target, const uint8_t size)
	{
		bool cts = 0;

		CsOn();

		// Send read command and check if available.
		SpiInstance->transfer(LoLaSi446x::COMMAND_READ_RX_FIFO);
		cts = SpiInstance->transfer(0xFF) == 0xFF;

		if (cts)
		{
			// Get response data
			for (uint8_t i = 0; i < size; i++)
			{
				target[i] = SpiInstance->transfer(0xFF);
			}
		}
		CsOff();
		return cts;
	}

public:
	const bool TryGetResponse(uint8_t* target, const uint8_t size)
	{
		bool cts = 0;

		CsOn();
		SpiInstance->transfer(LoLaSi446x::COMMAND_READ_BUFFER);
		cts = SpiInstance->transfer(0xFF) == 0xFF;
		if (cts)
		{
			for (uint8_t i = 0; i < size; i++)
			{
				target[i] = SpiInstance->transfer(0xFF);
			}
		}
		CsOff();

		return cts;
	}

	const bool TryWaitForResponse(uint8_t* target, const uint8_t size, const uint16_t timeoutMillis)
	{
		const uint32_t start = millis();

		while (!TryGetResponse(target, size))
		{
			if ((millis() - start) > timeoutMillis)
			{
				return false;
			}
			else
			{
				delayMicroseconds(5);
			}
		}

		return true;
	}

	const bool ClearToSend()
	{
		bool cts = 0;

		CsOn();
		SpiInstance->transfer(LoLaSi446x::COMMAND_READ_BUFFER);
		cts = SpiInstance->transfer(0xFF) == 0xFF;
		CsOff();

		return cts;
	}

protected:
	/// <summary>
	/// Read a fast response register.
	/// </summary>
	/// <param name="reg"></param>
	/// <returns></returns>
	const uint8_t GetFrr(const uint8_t reg)
	{
		uint_fast8_t frr = 0;

		CsOn();
		SpiInstance->transfer(reg);
		frr = SpiInstance->transfer(0xFF);
		CsOff();

		return frr;
	}

	const bool ClearFifo(const uint8_t fifoCommand)
	{
		MessageOut[0] = LoLaSi446x::COMMAND_FIFO_INFO;
		MessageOut[1] = fifoCommand;

		return SendRequest(MessageOut, 2);
	}

	const bool SetProperty(const uint16_t property, const uint16_t value)
	{
		MessageOut[0] = LoLaSi446x::COMMAND_SET_PROPERTY;
		MessageOut[1] = (uint8_t)(property >> 8);
		MessageOut[2] = 2;
		MessageOut[3] = (uint8_t)property;
		MessageOut[4] = (uint8_t)(value >> 8);
		MessageOut[5] = (uint8_t)value;

		return SendRequest(MessageOut, 6);
	}

	const bool GetProperty(const uint16_t property)
	{
		MessageOut[0] = LoLaSi446x::COMMAND_GET_PROPERTY;
		MessageOut[1] = (uint8_t)(property >> 8);
		MessageOut[2] = 2;
		MessageOut[3] = property;

		if (!SendRequest(LoLaSi446x::COMMAND_GET_PART_INFO))
		{
			Serial.println(F("GetPartInfo Failed 1."));
			return false;
		}

		return TryWaitForResponse(MessageIn, 2, 1);
	}

protected:
	const bool GetPartInfo(LoLaSi446x::Si446xInfoStruct& deviceInfo, const uint16_t timeoutMillis)
	{
		if (!SpinWaitClearToSend(timeoutMillis))
		{
			return false;
		}

		if (!SendRequest(LoLaSi446x::COMMAND_GET_PART_INFO))
		{
			Serial.println(F("GetPartInfo Failed 1."));
			return false;
		}

		if (!TryWaitForResponse(MessageIn, 8, timeoutMillis))
		{
			Serial.println(F("GetPartInfo Failed 2."));
			return false;
		}

		deviceInfo.ChipRevision = MessageIn[0];
		deviceInfo.PartId = ((uint16_t)MessageIn[1] << 8) | MessageIn[2];
		deviceInfo.PartBuild = MessageIn[3];
		deviceInfo.DeviceId = ((uint16_t)MessageIn[4] << 8) | MessageIn[5];
		deviceInfo.Customer = MessageIn[6];
		deviceInfo.RomId = MessageIn[7];

		if (!SpinWaitClearToSend(timeoutMillis))
		{
			Serial.println(F("GetPartInfo Failed 3."));
			return false;
		}

		if (!SendRequest(LoLaSi446x::COMMAND_FUNCTION_INFO))
		{
			Serial.println(F("GetPartInfo Failed 4."));
			return false;
		}

		if (!TryWaitForResponse(MessageIn, 6, timeoutMillis))
		{
			Serial.println(F("GetPartInfo Failed 5."));
			return false;
		}

		deviceInfo.RevisionExternal = MessageIn[0];
		deviceInfo.RevisionBranch = MessageIn[1];
		deviceInfo.RevisionInternal = MessageIn[2];
		deviceInfo.Patch = ((uint16_t)MessageIn[3] << 8) | MessageIn[4];
		deviceInfo.Function = MessageIn[5];

		return true;
	}



	const bool SetupCallbacks(const uint16_t timeoutMillis)
	{
		uint16_t packet = 0;
		uint16_t modem = 0;

		const uint8_t onFlag =
			LoLaSi446x::INTERRUPT_RX_BEGIN
			//| LoLaSi446x::INTERRUPT_RX_COMPLETE
			//| LoLaSi446x::INTERRUPT_RX_INVALID
			////| LoLaSi446x::INTERRUPT_SENT
			;


		if (!SpinWaitClearToSend(timeoutMillis))
		{
			return false;
		}

		// Read back properties to ensure everythin is as expected.
		if (!GetProperty(LoLaSi446x::CONFIG_INTERRUPTS_PHY_ENABLE))
		{
			return false;
		}

		packet = MessageIn[0];
		modem = MessageIn[1];

		Serial.println(F("Interrupt State Read start."));
		Serial.print(F("\tPacket 0b"));
		Serial.println(packet, BIN);
		Serial.print(F("\tModem 0b"));
		Serial.println(modem, BIN);

		const uint16_t flag = (onFlag) | ((uint16_t)(onFlag) << 8);
		//const uint16_t flag = (MessageIn[0] | lowFlag) | ((uint16_t)(MessageIn[1] | highFlag) << 8);

		if (!SpinWaitClearToSend(1))
		{
			return false;
		}

		// Write properties.
		if (!SetProperty(LoLaSi446x::CONFIG_INTERRUPTS_PHY_ENABLE, flag))
		{
			return false;
		}

		if (!SpinWaitClearToSend(timeoutMillis))
		{
			return false;
		}

		// Read back properties to ensure everythin is as expected.
		if (!GetProperty(LoLaSi446x::CONFIG_INTERRUPTS_PHY_ENABLE))
		{
			return false;
		}

		packet = MessageIn[0];
		modem = MessageIn[1];

		Serial.println(F("Interrupt State Read back."));
		Serial.print(F("\tPacket 0b"));
		Serial.println(packet, BIN);
		Serial.print(F("\tModem 0b"));
		Serial.println(modem, BIN);


		return true;
	}

	const bool ApplyStartupConfig(const uint8_t* configuration, const uint16_t configurationSize, const uint16_t timeoutMillis)
	{
		const uint32_t start = millis();

		for (uint_fast16_t i = 0; i < configurationSize; i++)
		{
			for (uint_fast8_t j = 0; j < 17; j++)
			{
				MessageOut[j] = configuration[i + j];
			}

			const uint8_t length = MessageOut[0];

			// Make sure it's ok to send a command.
			while (!ClearToSend())
			{
				if ((millis() - start) > timeoutMillis)
				{
					return false;
				}
			}

			CsOn();
			SpiInstance->transfer(&MessageOut[1], length);
			CsOff();

			i += length;
		}

		return true;
	}

private:
	const bool SendRequest(const uint8_t requestCode, const uint8_t* source, const uint8_t size)
	{
		if (ClearToSend())
		{
			CsOn();
			// Send request header.
			SpiInstance->transfer((uint8_t)requestCode);

			// Send request data
			SpiInstance->transfer((uint8_t*)source, size);
			CsOff();

			return true;
		}

		return false;
	}

	const bool SendRequest(const uint8_t* source, const uint8_t size)
	{
		if (ClearToSend())
		{
			CsOn();
			SpiInstance->transfer((uint8_t*)source, size);
			CsOff();

			return true;
		}

		return false;
	}

	const bool SendRequest(const uint8_t requestCode)
	{
		if (ClearToSend())
		{
			CsOn();
			SpiInstance->transfer(requestCode);
			CsOff();
			return true;
		}

		return false;
	}

	void CsOn()
	{
		digitalWrite(CsPin, LOW);
	}

	void CsOff()
	{
		digitalWrite(CsPin, HIGH);
	}
};
#endif