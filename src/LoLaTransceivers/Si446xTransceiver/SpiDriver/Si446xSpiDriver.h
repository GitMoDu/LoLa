// Si446xSpiDriver.h

#ifndef _SI446X_SPI_DRIVER_h
#define _SI446X_SPI_DRIVER_h

#include "Si446x.h"

#include <SPI.h>

using namespace Si446x;

template<const uint8_t pinCS,
	const uint8_t pinSDN,
	const uint8_t pinCLK = UINT8_MAX,
	const uint8_t pinMISO = UINT8_MAX,
	const uint8_t pinMOSI = UINT8_MAX,
	const uint8_t spiChannel = 0>
class Si446xSpiDriver
{
private:
	/// <summary>
	/// Max internal get/send message length.
	/// </summary>
	static constexpr uint8_t MAX_MESSAGE_SIZE = 8;

private:
	uint8_t Message[MAX_MESSAGE_SIZE]{};

private:
	SPIClass SpiInstance;

	const SPISettings Settings;

public:
	Si446xSpiDriver()
#if defined(ARDUINO_ARCH_ESP32)
		: SpiInstance(HSPI)
#elif defined(ARDUINO_ARCH_STM32F1)
		: SpiInstance(spiChannel)
#else
		: SpiInstance()
#endif
		, Settings(SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE)
	{}

public:
	void Stop()
	{
		SpiInstance.end();

		// Disable IO pins.
		pinMode(pinSDN, INPUT);
		CsOff();
		pinMode(pinCS, INPUT);
		digitalWrite(pinCS, HIGH);
	}

	const bool Start(const uint8_t* configuration, const size_t configurationSize)
	{
		// Setup IO pins.
		pinMode(pinSDN, INPUT);
		CsOff();
		pinMode(pinCS, OUTPUT);
		digitalWrite(pinCS, HIGH);

		Reset();

#if defined(ARDUINO_ARCH_ESP32)
		if (pinCS != UINT8_MAX
			&& pinCLK != UINT8_MAX
			&& pinMOSI != UINT8_MAX
			&& pinMISO != UINT8_MAX)
		{
			SpiInstance.begin((int8_t)pinCLK, (int8_t)pinMISO, (int8_t)pinMOSI, (int8_t)pinCS);
		}
		else if (pinCS != UINT8_MAX && pinCLK != UINT8_MAX)
		{
			SpiInstance.begin((int8_t)pinCLK, (int8_t)-1, (int8_t)-1, (int8_t)pinCS);
		}
		else if (pinCS != UINT8_MAX)
		{
			SpiInstance.begin((int8_t)-1, (int8_t)-1, (int8_t)-1, (int8_t)pinCS);
		}
#else
		SpiInstance.begin();
#endif

		PartInfoStruct partInfo{};
		if (!GetPartInfo(partInfo, 100000))
		{
			return false;
		}

		switch (partInfo.PartId)
		{
		case (uint16_t)Si446x::PART_NUMBER::SI4463:
			if (partInfo.DeviceId != (uint16_t)Si446x::DEVICE_ID::SI4463)
			{
#if defined(DEBUG_LOLA)
				Serial.print(F("DeviceId: "));
				Serial.println(partInfo.DeviceId);
#endif
				return false;
			}
			break;
		default:
#if defined(DEBUG_LOLA)
			Serial.print(F("Si446x Unknown Part Number: "));
			Serial.println(partInfo.PartId);
			Serial.print(F("DeviceId: "));
			Serial.println(partInfo.DeviceId);
#endif
			return false;
		}

		if (!ApplyConfiguration(configuration, configurationSize, 1000000))
		{
			return false;
		}

		if (!SetupCallbacks(true, 10000))
		{
			return false;
		}

		if (!ClearRadioEvents(10000))
		{
			return false;
		}

		if (!ClearFifo(FIFO_INFO_PROPERY::CLEAR_RX_TX, 2000))
		{
			return false;
		}

		if (!SetRadioState(RadioStateEnum::SLEEP, 10000))
		{
			return false;
		}

		if (!SpinWaitForResponse(10000))
		{
			return false;
		}

		return true;
	}

public:
	const bool ClearRxFifo(const uint32_t timeoutMicros = 500)
	{
		if (!SpinWaitForResponse(timeoutMicros))
		{
			return false;
		}

		return ClearFifo(FIFO_INFO_PROPERY::CLEAR_RX);
	}

	const bool ClearTxFifo(const uint32_t timeoutMicros)
	{
		return ClearFifo(FIFO_INFO_PROPERY::CLEAR_TX, timeoutMicros);
	}

	/// <summary>
	/// </summary>
	/// <param name="txPower">[0;127]</param>
	/// <param name="timeoutMicros"></param>
	/// <returns>True on success.</returns>
	const bool SetTxPower(const uint8_t txPower, const uint32_t timeoutMicros = 500)
	{
		return SetProperty(Property::PA_PWR_LVL, (uint8_t)(txPower & 0x7F), timeoutMicros);
	}

	const bool SetPacketSize(const uint8_t packetSize, const uint32_t timeoutMicros = 500)
	{
		return SetProperty(Property::PKT_FIELD_2_LENGTH_7_0, (uint8_t)packetSize, timeoutMicros);
	}

	const bool SetPacketSizeFull(const uint8_t packetSize, const uint32_t timeoutMicros = 500)
	{
		return SetProperty(Property::PKT_FIELD_2_LENGTH_12_8, (uint8_t)0, timeoutMicros)
			&& SetProperty(Property::PKT_FIELD_2_LENGTH_7_0, (uint8_t)packetSize, timeoutMicros);
	}

	const bool SetupCallbacks(const bool useDebugInterrupts = false, const uint32_t timeoutMicros = 10000)
	{
		if (useDebugInterrupts)
		{
			if (!SetProperty(Property::INT_CTL_PH_ENABLE, PH_FLAG_DEBUG, timeoutMicros))
			{
				return false;
			}

			if (!SetProperty(Property::INT_CTL_MODEM_ENABLE, MODEM_FLAG_DEBUG, timeoutMicros))
			{
				return false;
			}

			if (!SetProperty(Property::INT_CTL_CHIP_ENABLE, CHIP_FLAG_DEBUG, timeoutMicros))
			{
				return false;
			}
		}
		else
		{
			if (!SetProperty(Property::INT_CTL_PH_ENABLE, PH_FLAG, timeoutMicros))
			{
				return false;
			}

			if (!SetProperty(Property::INT_CTL_MODEM_ENABLE, MODEM_FLAG, timeoutMicros))
			{
				return false;
			}

			if (!SetProperty(Property::INT_CTL_CHIP_ENABLE, CHIP_FLAG, timeoutMicros))
			{
				return false;
			}
		}

		return true;
	}

public:
	const bool SetRadioState(const RadioStateEnum newState, const uint32_t timeoutMicros = 10000)
	{
		Message[0] = (uint8_t)Command::CHANGE_STATE;
		Message[1] = (const uint8_t)newState;

		return SendRequest(Message, 2, timeoutMicros);
	}

	const RadioStateEnum GetRadioState(const uint32_t timeoutMicros = 500)
	{
		if (!SendRequest(Command::REQUEST_DEVICE_STATE, timeoutMicros))
		{
			return RadioStateEnum::NO_CHANGE;
		}

		if (!SpinWaitForResponse(Message, 1, timeoutMicros))
		{
			return RadioStateEnum::NO_CHANGE;
		}

		return (RadioStateEnum)Message[0];
	}

	const RadioStateEnum GetRadioStateFast(const uint32_t timeoutMicros = 50)
	{
		return (RadioStateEnum)GetFrr((uint8_t)Command::FRR_B_READ);
	}

	const uint8_t GetRssiLatchFast()
	{
		return GetFrr((uint8_t)Command::FRR_A_READ);
	}

	const bool TryPrepareGetRadioEvents()
	{
		if (GetResponse())
		{
			Message[0] = (uint8_t)Command::GET_INT_STATUS;
			Message[1] = 0;
			Message[2] = 0;
			Message[3] = 0;

			CsOn();
			SpiInstance.transfer(Message, 4);
			CsOff();

			return true;
		}
		return false;
	}

	const bool TryGetRadioEvents(Si446x::RadioEventsStruct& radioEvents)
	{
		if (GetResponse(Message, 8))
		{
			radioEvents.SetFrom(Message, true);

			return true;
		}

		return false;
	}

	/// <summary>
	/// Read pending interrupts.
	/// Reading interrupts will also clear them.
	/// Tipically takes ~200 us of wait time after an interrupt has occurred.
	/// </summary>
	/// <param name="radioEvents"></param>
	/// <returns>True on success.</returns>
	const bool GetRadioEvents(Si446x::RadioEventsStruct& radioEvents, const uint32_t timeoutMicros = 1000)
	{
		Message[0] = (uint8_t)Command::GET_INT_STATUS;
		Message[1] = 0;
		Message[2] = 0;
		Message[3] = 0;

		if (!SpinWaitForResponse(timeoutMicros))
		{
			return false;
		}

		CsOn();
		SpiInstance.transfer(Message, 4);
		CsOff();

		if (!SpinWaitForResponse(Message, 8, timeoutMicros))
		{
			return false;
		}

		radioEvents.SetFrom(Message, false);

		return true;
	}

	const bool ClearRadioEvents(const uint32_t timeoutMicros = 1000)
	{
		Message[0] = (uint8_t)Command::GET_INT_STATUS;
		Message[1] = 0;
		Message[2] = 0;
		Message[3] = 0;

		if (!SpinWaitForResponse(timeoutMicros))
		{
			return false;
		}

		CsOn();
		SpiInstance.transfer(Message, 4);
		CsOff();

		if (!SpinWaitForResponse(Message, 8, timeoutMicros))
		{
			return false;
		}

		return true;
	}

	/// <summary>
	/// ClearRxFifo is not needed, packet handler's FIFO just keeps rolling.
	/// </summary>
	/// <param name="channel"></param>
	/// <param name="timeoutMicros"></param>
	/// <returns></returns>
	const bool RadioStartRx(const uint8_t channel, const uint32_t timeoutMicros = 500)
	{
		Message[0] = (uint8_t)Command::START_RX;
		Message[1] = channel;
		Message[2] = 0;
		Message[3] = 0;
		Message[4] = 0;
		Message[5] = (uint8_t)RadioStateEnum::NO_CHANGE; // RXTIMEOUT_STATE
		Message[6] = (uint8_t)RadioStateEnum::READY; // RXVALID_STATE
		Message[7] = (uint8_t)RadioStateEnum::RX; // RXINVALID_STATE

		return SendRequest(Message, 8, timeoutMicros);
	}

	/// <summary>
	///  ClearTxFifo is not needed, packet handler's FIFO just keeps rolling.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="size"></param>
	/// <param name="channel"></param>
	/// <param name="timeoutMicros"></param>
	/// <returns></returns>
	const bool RadioStartTx(const uint8_t* data, const uint8_t size, const uint8_t channel)
	{
		Message[0] = (uint8_t)Command::START_TX;
		Message[1] = channel;
		Message[2] = (uint8_t)RadioStateEnum::READY << 4;
		Message[3] = 0;
		Message[4] = 0;

		CsOn();
		SpiInstance.transfer((uint8_t)Command::WRITE_TX_FIFO);
		SpiInstance.transfer((uint8_t)size);
		SpiInstance.transfer((void*)data, (size_t)size);
		CsOff();

		//digitalWrite(7, HIGH);

		return SendRequest(Message, 5, 0);
	}

	const bool GetRxFifoCount(uint8_t& fifoCount, const uint32_t timeoutMicros = 500)
	{
		CsOn();
		SpiInstance.transfer((uint8_t)Command::READ_RX_FIFO);
		fifoCount = SpiInstance.transfer(0xFF);
		CsOff();

		return true;
	}

	const bool GetRxFifo(uint8_t* target, const uint8_t size)
	{
		CsOn();
		SpiInstance.transfer((uint8_t)Command::READ_RX_FIFO);
		for (uint8_t i = 0; i < size; i++)
		{
			target[i] = SpiInstance.transfer(0xFF);
		}
		CsOff();

		return true;
	}

	const bool GetResponse()
	{
		CsOn();
		SpiInstance.transfer((uint8_t)Command::READ_CMD_BUFF);
		const bool cts = SpiInstance.transfer(0xFF) == 0xFF;
		CsOff();

		return cts;
	}

	const bool GetResponse(uint8_t* target, const uint8_t size)
	{
		bool cts = 0;

		CsOn();
		SpiInstance.transfer((uint8_t)Command::READ_CMD_BUFF);
		cts = SpiInstance.transfer(0xFF) == 0xFF;
		if (cts)
		{
			for (uint8_t i = 0; i < size; i++)
			{
				target[i] = SpiInstance.transfer(0xFF);
			}
		}
		CsOff();

		return cts;
	}

	const bool SpinWaitForResponse(const uint32_t timeoutMicros = 500)
	{
		const uint32_t start = micros();

		while (!GetResponse())
		{
			if ((micros() - start) > timeoutMicros)
			{
				return false;
			}
			else
			{
				delayMicroseconds(1);
			}
		}

		return true;
	}

	const bool SpinWaitForResponse(uint8_t* target, const uint8_t size, const uint32_t timeoutMicros = 500)
	{
		const uint32_t start = micros();

		while (!GetResponse(target, size))
		{
			if ((micros() - start) > timeoutMicros)
			{
				return false;
			}
			else
			{
				delayMicroseconds(1);
			}
		}

		return true;
	}

private:
	const uint8_t GetFrr(const uint8_t reg)
	{
		uint_fast8_t frr = 0;

		CsOn();
		SpiInstance.transfer(reg);
		frr = SpiInstance.transfer(0xFF);
		CsOff();

		return frr;
	}

	const bool ClearFifo(const FIFO_INFO_PROPERY fifoCommand, const uint32_t timeoutMicros = 500)
	{
		Message[0] = (uint8_t)Command::FIFO_INFO;
		Message[1] = (uint8_t)fifoCommand;

		return SendRequest(Message, 2, timeoutMicros);
	}

	const bool SetProperty(const Property property, const uint16_t value, const uint32_t timeoutMicros = 500)
	{
		Message[0] = (uint8_t)Command::SET_PROPERTY;
		Message[1] = (uint8_t)((uint16_t)property >> 8);
		Message[2] = 2;
		Message[3] = (uint8_t)property;
		Message[4] = (uint8_t)(value >> 8);
		Message[5] = (uint8_t)value;

		return SendRequest(Message, 6, timeoutMicros);
	}

	const bool SetProperty(const Property property, const uint8_t value, const uint32_t timeoutMicros = 500)
	{
		Message[0] = (uint8_t)Command::SET_PROPERTY;
		Message[1] = (uint8_t)((uint16_t)property >> 8);
		Message[2] = 1;
		Message[3] = (uint8_t)property;
		Message[4] = (uint8_t)value;

		return SendRequest(Message, 5, timeoutMicros);
	}

	const bool GetProperty(const Property property, const uint32_t timeoutMicros = 500)
	{
		Message[0] = (uint8_t)Command::GET_PROPERTY;
		Message[1] = (uint8_t)((uint16_t)property >> 8);
		Message[2] = 2;
		Message[3] = (uint8_t)property;

		if (!SendRequest(Message, 4, timeoutMicros))
		{
			return false;
		}

		return SpinWaitForResponse(Message, 2, timeoutMicros);
	}

	const bool GetPartInfo(PartInfoStruct& partInfo, const uint32_t timeoutMicros)
	{
		if (!SendRequest(Command::PART_INFO, timeoutMicros))
		{
			return false;
		}

		if (!SpinWaitForResponse(Message, 8, timeoutMicros))
		{
			return false;
		}

		partInfo.SetFrom(Message);

		return true;
	}

	const bool GetFunctionInfo(FunctionInfoStruct& functionInfo, const uint32_t timeoutMicros)
	{
		if (!SendRequest(Command::FUNC_INFO))
		{
			return false;
		}

		if (!SpinWaitForResponse(Message, 6, timeoutMicros))
		{
			return false;
		}

		functionInfo.SetFrom(Message);

		return true;
	}

	const bool ApplyConfiguration(const uint8_t* configuration, const size_t configurationSize, const uint32_t timeoutMicros)
	{
		const uint32_t start = micros();

		size_t i = 0;
		bool success = false;
		while (i < configurationSize
			&& ((micros() - start) < timeoutMicros))
		{
			if (GetResponse())
			{
				const size_t length = configuration[i];

				CsOn();
				SpiInstance.transfer((void*)&configuration[i + 1], length);
				CsOff();

				i += 1 + length;
				if (!success)
				{
					success = i == configurationSize;
					if (!success
						&& i > configurationSize)
					{
						return false;
					}
				}
			}
		}

		return success;
	}

	const bool SendRequest(const Command requestCode, const uint8_t* source, const uint8_t size, const uint32_t timeoutMicros = 500)
	{
		if (SpinWaitForResponse(timeoutMicros))
		{
			CsOn();
			// Send request header.
			SpiInstance.transfer((uint8_t)requestCode);

			// Send request data
			SpiInstance.transfer((uint8_t*)source, size);
			CsOff();

			return true;
		}

		return false;
	}

	const bool SendRequest(const Command requestCode, const uint32_t timeoutMicros = 500)
	{
		if (SpinWaitForResponse(timeoutMicros))
		{
			CsOn();
			SpiInstance.transfer((uint8_t)requestCode);
			CsOff();
			return true;
		}

		return false;
	}

	const bool SendRequest(const uint8_t* source, const uint8_t size, const uint32_t timeoutMicros = 500)
	{
		if (SpinWaitForResponse(timeoutMicros))
		{
			CsOn();
			SpiInstance.transfer((uint8_t*)source, size);
			CsOff();

			return true;
		}

		return false;
	}

	void CsOn()
	{
		digitalWrite(pinCS, LOW);
		SpiInstance.beginTransaction(Settings);
	}

	void CsOff()
	{
		SpiInstance.endTransaction();
		digitalWrite(pinCS, HIGH);
	}

	/// <summary>
	/// Reset the RF chip.
	/// </summary>
	void Reset()
	{
		pinMode(pinSDN, OUTPUT);
		digitalWrite(pinSDN, HIGH);
		delay(10);
		digitalWrite(pinSDN, LOW);
		delay(10);
	}
};
#endif