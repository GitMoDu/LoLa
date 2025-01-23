// Si446xSpiDriver.h

#ifndef _SI446X_SPI_DRIVER_h
#define _SI446X_SPI_DRIVER_h


#include <SPI.h>
#include "Si446x.h"

template<const uint8_t pinCS,
	const uint8_t pinSDN>
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

	const uint8_t PacketHandlerInterruptFlags;
	const uint8_t ModemInterruptFlags;
	const uint8_t ChipInterruptFlags;


public:
	Si446xSpiDriver(SPIClass& spiInstance,
		const uint8_t packetHandlerInterruptFlags = (uint8_t)Si446x::INT_CTL_PH::PACKET_SENT_EN | (uint8_t)Si446x::INT_CTL_PH::PACKET_RX_EN,
		const uint8_t modemInterruptFlags = 0,
		const uint8_t chipInterruptFlags = 0)
		: SpiInstance(spiInstance)
		, Settings(Si446x::SPI_CLOCK_SPEED, Si446x::SPI_ORDER, Si446x::SPI_MODE)
		, PacketHandlerInterruptFlags(packetHandlerInterruptFlags)
		, ModemInterruptFlags(modemInterruptFlags)
		, ChipInterruptFlags(chipInterruptFlags)
	{
	}

public:
	void Stop()
	{
		// Disable IO pins.
		pinMode(pinSDN, OUTPUT);
		pinMode(pinCS, OUTPUT);
		CsOff();
		digitalWrite(pinSDN, LOW);
	}

	const bool Start(const uint8_t* configuration, const size_t configurationSize, const uint8_t* patch = nullptr, const size_t patchSize = 0)
	{
		// Setup IO pins.
		pinMode(pinSDN, OUTPUT);
		pinMode(pinCS, OUTPUT);
		digitalWrite(pinSDN, LOW);
		CsOff();

		// Reset the RF chip according to AN633.
		digitalWrite(pinSDN, HIGH);
		delayMicroseconds(15);
		digitalWrite(pinSDN, LOW);
		delay(15);

		// Push ROM Patch before Power on command.
		if (patch != nullptr
			&& patchSize > 0)
		{
			CsOn();
			SpiInstance.transfer((void*)patch, patchSize);
			CsOff();
#if defined(DEBUG_LOLA)
			Serial.print(F("Patch Applied ("));
			Serial.print(patchSize);
			Serial.println(F(" bytes)"));
#endif
		}

		if (!ApplyConfiguration(configuration, configurationSize, 1000000))
		{
			return false;
		}

		// Wait for CTS and read part info.
		Si446x::PartInfoStruct partInfo{};
		if (!GetPartInfo(partInfo, 200000))
		{
			return false;
		}

		switch (partInfo.PartId)
		{
		case (uint16_t)Si446x::PART_NUMBER::SI4463:
			if (partInfo.DeviceId == (uint8_t)Si446x::DEVICE_ID::SI4463
				&& partInfo.RomId == (uint8_t)Si446x::ROM_ID::SI4463)
			{
#if defined(DEBUG_LOLA)
				Serial.print(F("PartId: "));
				Serial.println(partInfo.PartId);
				Serial.print(F("DeviceId: "));
				Serial.println(partInfo.DeviceId);
				Serial.print(F("RomId: "));
				Serial.println(partInfo.RomId);
				Serial.print(F("ChipRevision: "));
				Serial.println(partInfo.ChipRevision);
#endif
				break;
			}
			else
			{
#if defined(DEBUG_LOLA)
				Serial.print(F("Device mismatch "));
				Serial.print(F("PartId: "));
				Serial.println(partInfo.PartId);
				Serial.print(F("DeviceId: "));
				Serial.println(partInfo.DeviceId);
				Serial.print(F("RomId: "));
				Serial.println(partInfo.RomId);
#endif
				return false;
			}
			break;
		default:
#if defined(DEBUG_LOLA)
			Serial.print(F("Device mismatch "));
			Serial.print(F("PartId: "));
			Serial.println(partInfo.PartId);
			Serial.print(F("DeviceId: "));
			Serial.println(partInfo.DeviceId);
			Serial.print(F("RomId: "));
			Serial.println(partInfo.RomId);
#endif
			return false;
		}

		if (!SetupCallbacks(2000))
		{
			return false;
		}

		if (!ClearRxTxFifo(2000))
		{
			return false;
		}

		if (!ClearInterrupts(2000))
		{
			return false;
		}

		static constexpr Si446x::RadioStateEnum startState = Si446x::RadioStateEnum::READY;

		if (!SetRadioState(startState, 10000))
		{
			return false;
		}

		delay(1);
		Si446x::RadioStateEnum checkState{};
		if (GetRadioState(checkState, 10000)
			&& startState != checkState)
		{
			return false;
		}

		return true;
	}

public:
	const bool ClearRxFifo(const uint32_t timeoutMicros = 0)
	{
		return ClearFifo(Si446x::FIFO_INFO_PROPERY::CLEAR_RX, timeoutMicros);
	}

	const bool ClearTxFifo(const uint32_t timeoutMicros)
	{
		return ClearFifo(Si446x::FIFO_INFO_PROPERY::CLEAR_TX, timeoutMicros);
	}

	const bool ClearRxTxFifo(const uint32_t timeoutMicros)
	{
		return ClearFifo(Si446x::FIFO_INFO_PROPERY::CLEAR_RX_TX, timeoutMicros);
	}

	/// <summary>
	/// </summary>
	/// <param name="txPower">[0;SI4463_TRANSMIT_POWER_MAX]</param>
	/// <param name="timeoutMicros"></param>
	/// <returns>True on success.</returns>
	const bool SetTxPower(const uint8_t txPower, const uint32_t timeoutMicros = 0)
	{
		return SetProperty(Si446x::Property::PA_PWR_LVL, (uint8_t)(txPower & Si446x::TRANSMIT_POWER_MAX), timeoutMicros);
	}

	const bool SetPacketSize(const uint8_t packetSize, const uint32_t timeoutMicros = 0)
	{
		return SetProperty(Si446x::Property::PKT_FIELD_2_LENGTH_7_0, (uint8_t)packetSize, timeoutMicros);
	}

	const bool SetPacketSizeFull(const uint8_t packetSize, const uint32_t timeoutMicros = 0)
	{
		return SetProperty(Si446x::Property::PKT_FIELD_2_LENGTH_12_8, (uint8_t)0, timeoutMicros)
			&& SetProperty(Si446x::Property::PKT_FIELD_2_LENGTH_7_0, (uint8_t)packetSize, timeoutMicros);
	}

	const bool SetupCallbacks(const uint32_t timeoutMicros = 0)
	{
		if (!SetProperty(Si446x::Property::INT_CTL_PH_ENABLE, PacketHandlerInterruptFlags, timeoutMicros))
		{
			return false;
		}

		if (!SetProperty(Si446x::Property::INT_CTL_MODEM_ENABLE, ModemInterruptFlags, timeoutMicros))
		{
			return false;
		}

		if (!SetProperty(Si446x::Property::INT_CTL_CHIP_ENABLE, ChipInterruptFlags, timeoutMicros))
		{
			return false;
		}

		return true;
	}

public:
	const bool SetRadioState(const Si446x::RadioStateEnum newState, const uint32_t timeoutMicros = 0)
	{
		Message[0] = (uint8_t)Si446x::Command::CHANGE_STATE;
		Message[1] = (const uint8_t)newState;

		return SendRequest(Message, 2, timeoutMicros);
	}

	const bool GetRadioState(Si446x::RadioStateEnum& radioState, const uint32_t timeoutMicros = 0)
	{
		if (!SendRequest(Si446x::Command::REQUEST_DEVICE_STATE, timeoutMicros))
		{
			return false;
		}

		if (!SpinWaitForResponse(Message, 1, timeoutMicros))
		{
			return false;
		}

		radioState = (Si446x::RadioStateEnum)Message[0];

		return true;
	}

	const bool ClearInterrupts(const uint32_t timeoutMicros = 0)
	{
		Message[0] = (uint8_t)Si446x::Command::GET_INT_STATUS;
		Message[1] = 0;// Packet Handler interrupts.
		Message[2] = 0;// Modem interrupts.
		Message[3] = 0;// Chip interrupts.

		if (!SpinWaitForResponse(timeoutMicros))
		{
			return false;

		}

		CsOn();
		SpiInstance.transfer(Message, 4);
		CsOff();

		return true;
	}

	/// <summary>
	/// ClearRxFifo is not needed, packet handler's FIFO just keeps rolling.
	/// </summary>
	/// <param name="channel"></param>
	/// <param name="timeoutMicros"></param>
	/// <returns></returns>
	const bool RadioStartRx(const uint8_t channel, const uint32_t timeoutMicros = 0)
	{
		Message[0] = (uint8_t)Si446x::Command::START_RX;
		Message[1] = channel;
		Message[2] = 0;
		Message[3] = 0;
		Message[4] = 0;
		Message[5] = (uint8_t)Si446x::RadioStateEnum::RX; // RXTIMEOUT_STATE
		Message[6] = (uint8_t)Si446x::RadioStateEnum::READY; // RXVALID_STATE
		Message[7] = (uint8_t)Si446x::RadioStateEnum::RX; // RXINVALID_STATE

		return SendRequest(Message, 8, timeoutMicros);
	}

	const bool RadioStartTx(const uint8_t channel, const uint32_t timeoutMicros = 0)
	{
		Message[0] = (uint8_t)Si446x::Command::START_TX;
		Message[1] = channel;
		Message[2] = (uint8_t)Si446x::RadioStateEnum::TX_TUNE << 4; // TXCOMPLETE_STATE
		Message[3] = 0;
		Message[4] = 0;
		Message[5] = 0;
		Message[6] = 0;

		return SendRequest(Message, 7, timeoutMicros);
	}

	const bool RadioTxWrite(const uint8_t* data, const uint8_t size, const uint32_t timeoutMicros = 0)
	{
		if (!SpinWaitForResponse(timeoutMicros))
		{
			return false;
		}

		CsOn();
		SpiInstance.transfer((uint8_t)Si446x::Command::WRITE_TX_FIFO);
		SpiInstance.transfer((uint8_t)size);
		SpiInstance.transfer((void*)data, (size_t)size);
		CsOff();

		return true;
	}

	const bool GetRxFifoCount(uint8_t& fifoCount, const uint32_t timeoutMicros = 0)
	{
		if (!SpinWaitForResponse(timeoutMicros))
		{
			fifoCount = 0;
			return false;
		}

		CsOn();
		SpiInstance.transfer((uint8_t)Si446x::Command::READ_RX_FIFO);
		fifoCount = SpiInstance.transfer(0xFF);
		CsOff();

		return true;
	}

	const bool GetRxFifo(uint8_t* target, const uint8_t size, const uint32_t timeoutMicros = 0)
	{
		if (SpinWaitForResponse(timeoutMicros))
		{
			CsOn();
			SpiInstance.transfer((uint8_t)Si446x::Command::READ_RX_FIFO);
			for (uint8_t i = 0; i < size; i++)
			{
				target[i] = SpiInstance.transfer(0xFF);
			}
			CsOff();

			return true;
		}

		return false;
	}

	const bool GetResponse()
	{
		CsOn();
		SpiInstance.transfer((uint8_t)Si446x::Command::READ_CMD_BUFF);
		const bool cts = SpiInstance.transfer(0xFF) == 0xFF;
		CsOff();

		return cts;
	}

	const bool GetResponse(uint8_t* target, const uint8_t size)
	{
		bool cts = 0;

		CsOn();
		SpiInstance.transfer((uint8_t)Si446x::Command::READ_CMD_BUFF);
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

	const bool SpinWaitForResponse(const uint32_t timeoutMicros = 0)
	{
		if (timeoutMicros > 0)
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
					delayMicroseconds(5);
				}
			}

			return true;
		}
		else
		{
			return GetResponse();
		}
	}

	const bool SpinWaitForResponse(uint8_t* target, const uint8_t size, const uint32_t timeoutMicros = 0)
	{
		if (timeoutMicros > 0)
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
					delayMicroseconds(5);
				}
			}

			return true;
		}
		else
		{
			return GetResponse(target, size);
		}
	}

	const uint8_t GetFrr(const uint8_t index)
	{
		uint_fast8_t frr = 0;

		CsOn();
		SpiInstance.transfer(index);
		frr = SpiInstance.transfer(0xFF);
		CsOff();

		return frr;
	}

private:
	const bool ClearFifo(const Si446x::FIFO_INFO_PROPERY fifoCommand, const uint32_t timeoutMicros = 0)
	{
		Message[0] = (uint8_t)Si446x::Command::FIFO_INFO;
		Message[1] = (uint8_t)fifoCommand;

		return SendRequest(Message, 2, timeoutMicros);
	}

	const bool SetProperty(const Si446x::Property property, const uint16_t value, const uint32_t timeoutMicros = 0)
	{
		Message[0] = (uint8_t)Si446x::Command::SET_PROPERTY;
		Message[1] = (uint8_t)((uint16_t)property >> 8);
		Message[2] = 2;
		Message[3] = (uint8_t)property;
		Message[4] = (uint8_t)(value >> 8);
		Message[5] = (uint8_t)value;

		return SendRequest(Message, 6, timeoutMicros);
	}

	const bool SetProperty(const Si446x::Property property, const uint8_t value, const uint32_t timeoutMicros = 0)
	{
		Message[0] = (uint8_t)Si446x::Command::SET_PROPERTY;
		Message[1] = (uint8_t)((uint16_t)property >> 8);
		Message[2] = 1;
		Message[3] = (uint8_t)property;
		Message[4] = (uint8_t)value;

		return SendRequest(Message, 5, timeoutMicros);
	}

	const bool GetProperty(const Si446x::Property property, const uint32_t timeoutMicros = 0)
	{
		Message[0] = (uint8_t)Si446x::Command::GET_PROPERTY;
		Message[1] = (uint8_t)((uint16_t)property >> 8);
		Message[2] = 2;
		Message[3] = (uint8_t)property;

		if (!SendRequest(Message, 4, timeoutMicros))
		{
			return false;
		}

		return SpinWaitForResponse(Message, 2, timeoutMicros);
	}

	const bool GetPartInfo(Si446x::PartInfoStruct& partInfo, const uint32_t timeoutMicros)
	{
		if (!SendRequest(Si446x::Command::PART_INFO, timeoutMicros))
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

	const bool GetFunctionInfo(Si446x::FunctionInfoStruct& functionInfo, const uint32_t timeoutMicros)
	{
		if (!SendRequest(Si446x::Command::FUNC_INFO))
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

	const bool SendRequest(const Si446x::Command requestCode, const uint8_t* source, const uint8_t size, const uint32_t timeoutMicros = 500)
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

	const bool SendRequest(const Si446x::Command requestCode, const uint32_t timeoutMicros = 500)
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
};
#endif