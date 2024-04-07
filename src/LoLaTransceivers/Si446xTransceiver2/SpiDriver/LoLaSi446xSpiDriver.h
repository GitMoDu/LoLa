// LoLaSi446xSpiDriver.h

#ifndef _LOLA_SI446X_SPI_DRIVER_h
#define _LOLA_SI446X_SPI_DRIVER_h

#include "LoLaSi446x.h"

#include <SPI.h>

using namespace LoLaSi446x;

/// <summary>
/// </summary>
/// <typeparam name="CsPin"></typeparam>
template<const uint8_t CsPin,
	const uint8_t SdnPin>
class LoLaSi446xSpiDriver
{
private:
	/// <summary>
	/// Max internal get/send message length.
	/// </summary>
	static constexpr uint8_t MAX_MESSAGE_SIZE = 8;

	// The SPI interface is designed to operate at a maximum of 10 MHz.
	// 72 MHz / 8 = 9 MHz
	static constexpr uint32_t SPI_CLOCK_SPEED = 9000000;

private:
	uint8_t Message[MAX_MESSAGE_SIZE]{};

private:
	SPIClass* SpiInstance;

	const SPISettings Settings;

public:
	LoLaSi446xSpiDriver(SPIClass* spiInstance)
		: SpiInstance(spiInstance)
		, Settings(SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE0)
	{}

public:
	void Stop()
	{
		if (SpiInstance != nullptr)
		{
			// TODO: Send shutdown command
		}

		// Disable IO pins.
		pinMode(SdnPin, INPUT);
		CsOff();
		pinMode(CsPin, INPUT);
	}

	const bool Start(const uint8_t* configuration, const size_t configurationSize)
	{
		if (!Start())
		{
			return false;
		}

		LoLaSi446x::Si446xInfoStruct deviceInfo{};
		if (!GetPartInfo(deviceInfo, 100000))
		{
			return false;
		}

		switch (deviceInfo.PartId)
		{
		case (uint16_t)LoLaSi446x::PART_NUMBER::SI4463:
			if (deviceInfo.DeviceId != (uint16_t)LoLaSi446x::DEVICE_ID::SI4463)
			{
#if defined(DEBUG_LOLA)
				Serial.print(F("DeviceId: "));
				Serial.println(deviceInfo.DeviceId);
#endif
				return false;
			}
			break;
		default:
#if defined(DEBUG_LOLA)
			Serial.print(F("Si446xSpiDriver Unknown Part Number: "));
			Serial.println(deviceInfo.PartId);
			Serial.print(F("DeviceId: "));
			Serial.println(deviceInfo.DeviceId);
#endif
			return false;
		}

		if (!ApplyConfiguration(configuration, configurationSize, 1000000))
		{
			return false;
		}

		if (!SetupCallbacks())
		{
			return false;
		}

		LoLaSi446x::RadioEventsStruct radioEvents{};
		if (!GetRadioEvents(radioEvents, 10000))
		{
			return false;
		}

		if (!ClearFifo(FIFO_INFO_PROPERY::CLEAR_RX_TX, 2000))
		{
			return false;
		}

		// Sleep until first command.
		if (!SetRadioState(RadioStateEnum::SLEEP, 10000))
		{
			return false;
		}

		if (!SpinWaitClearToSend(10000))
		{
			return false;
		}

		if (GetRadioStateFast() != RadioStateEnum::SLEEP)
		{
			return false;
		}

		if (!SpinWaitClearToSend(10000))
		{
			return false;
		}

		return true;
	}

	const uint8_t GetLatchedRssi()
	{
		return GetFrr(Command::FRR_A_READ);
	}

	const bool ClearRxFifo(const uint32_t timeoutMicros = 500)
	{
		if (!SpinWaitClearToSend(timeoutMicros))
		{
			return false;
		}

		return ClearFifo(FIFO_INFO_PROPERY::CLEAR_RX);
	}

	const bool ClearTxFifo(const uint32_t timeoutMicros)
	{
		return ClearFifo(FIFO_INFO_PROPERY::CLEAR_TX, timeoutMicros);
	}

	const bool SetTxPower(const uint8_t txPower, const uint32_t timeoutMicros = 500)
	{
		return SetProperty(Property::PA_PWR_LVL, (uint16_t)txPower, timeoutMicros);
	}

	const bool SetPacketSize(const uint8_t packetSize, const uint32_t timeoutMicros = 500)
	{
		return SetProperty(Property::PKT_FIELD_2_LENGTH_12_8, (uint8_t)0, timeoutMicros)
			&& SetProperty(Property::PKT_FIELD_2_LENGTH_7_0, (uint8_t)packetSize, timeoutMicros);
	}

	const bool SetupCallbacks()
	{
		const uint8_t phFlag = (uint8_t)INT_CTL_PH::PACKET_RX_EN | (uint8_t)INT_CTL_PH::PACKET_SENT_EN;
		if (!SetProperty(Property::INT_CTL_PH_ENABLE, phFlag, 10000))
		{
			return false;
		}

		const uint8_t modemFlag = 0;
		if (!SetProperty(Property::INT_CTL_MODEM_ENABLE, modemFlag, 10000))
		{
			return false;
		}

		const uint8_t chipFlag = (uint8_t)INT_CTL_CHIP::LOW_BATT_EN;
		if (!SetProperty(Property::INT_CTL_CHIP_ENABLE, chipFlag, 10000))
		{
			return false;
		}

		return true;
	}

public:
	const bool SpinWaitClearToSend(const uint32_t timeoutMicros)
	{
		const uint32_t start = micros();
		while (!ClearToSend())
		{
			if (micros() - start > timeoutMicros)
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
		if (!SpinWaitClearToSend(timeoutMicros))
		{
			return RadioStateEnum::NO_CHANGE;
		}

		return (RadioStateEnum)GetFrr((uint8_t)Command::FRR_B_READ);
	}

	const bool GetRssiLatchFast(uint8_t& rssiLatch, const uint32_t timeoutMicros = 50)
	{
		if (!SpinWaitClearToSend(timeoutMicros))
		{
			rssiLatch = 0;
			return false;
		}

		rssiLatch = GetFrr((uint8_t)Command::FRR_A_READ);

		return true;
	}

	const bool TryPrepareGetRadioEvents()
	{
		if (ClearToSend())
		{
			Message[0] = (uint8_t)Command::GET_INT_STATUS;
			Message[1] = 0;
			Message[2] = 0;
			Message[3] = 0xFF;

			CsOn();
			SpiInstance->transfer(Message, 4);
			CsOff();

			return true;
		}
		return false;
	}

	const bool TryGetRadioEvents(LoLaSi446x::RadioEventsStruct& radioEvents)
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
	const bool GetRadioEvents(LoLaSi446x::RadioEventsStruct& radioEvents, const uint32_t timeoutMicros = 1000)
	{
		Message[0] = (uint8_t)Command::GET_INT_STATUS;
		Message[1] = 0;
		Message[2] = 0;
		Message[3] = 0xFF;

		if (!SpinWaitClearToSend(timeoutMicros))
		{
			return false;
		}

		CsOn();
		SpiInstance->transfer(Message, 4);
		CsOff();

		if (!SpinWaitForResponse(Message, 8, timeoutMicros))
		{
			return false;
		}

		radioEvents.SetFrom(Message, false);

		return true;
	}

	const bool RadioStartRx(const uint8_t channel, const uint32_t timeoutMicros = 1000)
	{
		// Not need, spin send request will have the same effect.
		//if (!SetRadioState(RadioStateEnum::SPI_ACTIVE, timeoutMicros))
		//{
		// #if defined(DEBUG_LOLA)
		//	Serial.println(F("Rx failed 2"));
		// #endif
		//	return false;
		//}

		// Not needed.
		//if (!ClearRxFifo(timeoutMicros))
		//{
		// #if defined(DEBUG_LOLA)
		//	Serial.println(F("Rx failed 2"));
		// #endif
		//	return false;
		//}

		Message[0] = (uint8_t)Command::START_RX;
		Message[1] = channel;
		Message[2] = 0;
		Message[3] = 0;
		Message[4] = 0;
		Message[5] = (uint8_t)RadioStateEnum::NO_CHANGE; // RXTIMEOUT_STATE
		Message[6] = (uint8_t)RadioStateEnum::READY; // RXVALID_STATE
		Message[7] = (uint8_t)RadioStateEnum::SLEEP; // RXINVALID_STATE

		return SendRequest(Message, 8, timeoutMicros);
	}

	const bool RadioStartTx(const uint8_t* data, const uint8_t size, const uint8_t channel, const uint32_t timeoutMicros = 500)
	{
		if (!SpinWaitClearToSend(timeoutMicros))
		{
			Serial.println(F("Tx Wait failed."));

			return false;
		}

		CsOn();
		SpiInstance->transfer((uint8_t)Command::WRITE_TX_FIFO);
		SpiInstance->transfer((uint8_t)size);
		SpiInstance->transfer((void*)data, (size_t)size);
		CsOff();

		if (!SetPacketSize(size, timeoutMicros))
		{
			Serial.println(F("SetPacketSize failed."));
			return false;
		}

		Message[0] = (uint8_t)Command::START_TX;
		Message[1] = channel;
		Message[2] = (uint8_t)RadioStateEnum::READY << 4;// | 1 << 0 | 1 << 1; // On transmitted restore state.
		Message[3] = 0;
		Message[4] = 0;

		if (!SendRequest(Message, 5, timeoutMicros))
		{
			Serial.println(F("Tx failed."));
			return false;
		}

		//TODO: Skip wait for CLS.
		if (!SpinWaitClearToSend(timeoutMicros))
		{
			Serial.println(F("Tx post failed."));
			return false;
		}

		if (!SetPacketSize(32, timeoutMicros))
		{
			Serial.println(F("Tx restore failed."));
			return false;
		}

		return true;
	}

	const bool GetRxFifo(uint8_t* target, const uint8_t size)
	{
		CsOn();
		SpiInstance->transfer((uint8_t)Command::READ_RX_FIFO);
		for (uint8_t i = 0; i < size; i++)
		{
			target[i] = SpiInstance->transfer(0xFF);
		}
		CsOff();

		return true;
	}

	const bool ClearToSend()
	{
		bool cts = 0;

		CsOn();
		SpiInstance->transfer((uint8_t)Command::READ_CMD_BUFF);
		cts = SpiInstance->transfer(0xFF) == 0xFF;
		CsOff();

		return cts;
	}

	const bool GetResponse(uint8_t* target, const uint8_t size)
	{
		bool cts = 0;

		CsOn();
		SpiInstance->transfer((uint8_t)Command::READ_CMD_BUFF);
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


	const bool GetRxFifoCount(uint8_t& fifoCount, const uint32_t timeoutMicros = 500)
	{
		if (!SpinWaitClearToSend(timeoutMicros))
		{
			fifoCount = 0;
			return false;
		}

		CsOn();
		SpiInstance->transfer((uint8_t)Command::READ_RX_FIFO);
		fifoCount = SpiInstance->transfer(0xFF);
		CsOff();

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

	/// <summary>
	/// This command is normally used for error recovery, fifo hardware does not need to be reset prior to use.
	/// </summary>
	/// <param name="fifoCommand"></param>
	/// <returns></returns>
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

	const bool GetPartInfo(LoLaSi446x::Si446xInfoStruct& deviceInfo, const uint32_t timeoutMicros)
	{
		if (!SpinWaitClearToSend(timeoutMicros))
		{
			return false;
		}

		if (!SendRequest(Command::PART_INFO))
		{
			return false;
		}

		if (!SpinWaitForResponse(Message, 8, timeoutMicros))
		{
			return false;
		}

		deviceInfo.ChipRevision = Message[0];
		deviceInfo.PartId = ((uint16_t)Message[1] << 8) | Message[2];
		deviceInfo.PartBuild = Message[3];
		deviceInfo.DeviceId = ((uint16_t)Message[4] << 8) | Message[5];
		deviceInfo.Customer = Message[6];
		deviceInfo.RomId = Message[7];

		if (!SpinWaitClearToSend(timeoutMicros))
		{
			return false;
		}

		if (!SendRequest(Command::FUNC_INFO))
		{
			return false;
		}

		if (!SpinWaitForResponse(Message, 6, timeoutMicros))
		{
			return false;
		}

		deviceInfo.RevisionExternal = Message[0];
		deviceInfo.RevisionBranch = Message[1];
		deviceInfo.RevisionInternal = Message[2];
		deviceInfo.Patch = ((uint16_t)Message[3] << 8) | Message[4];
		deviceInfo.Function = Message[5];

		return true;
	}


	const bool ApplyConfiguration(const uint8_t* configuration, const uint16_t configurationSize, const uint32_t timeoutMicros)
	{
		const uint32_t start = micros();

		uint_fast16_t i = 0;
		bool success = false;
		while (i < configurationSize
			&& ((micros() - start) < timeoutMicros))
		{
			if (ClearToSend())
			{
				const uint8_t length = configuration[i];

				CsOn();
				SpiInstance->transfer((void*)&configuration[i + 1], (size_t)length);
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

	const bool Start()
	{
		// Setup IO pins.
		pinMode(SdnPin, INPUT);
		CsOff();
		pinMode(CsPin, OUTPUT);

		if (SpiInstance == nullptr)
		{
			return false;
		}

		//TODO: can be much shorter? Keep SDN Active, no need to reset on each link start?
		Reset();

		SpiInstance->begin();

		return true;
	}

	const bool SendRequest(const Command requestCode, const uint8_t* source, const uint8_t size, const uint32_t timeoutMicros = 500)
	{
		if (SpinWaitClearToSend(timeoutMicros))
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

	const bool SendRequest(const Command requestCode, const uint32_t timeoutMicros = 500)
	{
		if (SpinWaitClearToSend(timeoutMicros))
		{
			CsOn();
			SpiInstance->transfer((uint8_t)requestCode);
			CsOff();
			return true;
		}

		return false;
	}

	const bool SendRequest(const uint8_t* source, const uint8_t size, const uint32_t timeoutMicros = 500)
	{
		if (SpinWaitClearToSend(timeoutMicros))
		{
			CsOn();
			SpiInstance->transfer((uint8_t*)source, size);
			CsOff();

			return true;
		}

		return false;
	}

	void CsOn()
	{
		digitalWrite(CsPin, LOW);
		SpiInstance->beginTransaction(Settings);
	}

	void CsOff()
	{
		SpiInstance->endTransaction();
		digitalWrite(CsPin, HIGH);
	}

	/// <summary>
	/// Reset the RF chip.
	/// </summary>
	void Reset()
	{
		pinMode(SdnPin, OUTPUT);
		digitalWrite(SdnPin, HIGH);
		delay(10);
		digitalWrite(SdnPin, LOW);
		delay(10);
	}
};
#endif