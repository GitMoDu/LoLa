// LoLaSi446xRadioDriver.h

#ifndef _LOLASI446XRADIODRIVER_h
#define _LOLASI446XRADIODRIVER_h


#include <PacketDriver\RadioDrivers\LoLaSi446x\LoLaSi446xSPIDriver.h>
#include <PacketDriver\RadioDrivers\LoLaSi446x\radio_config.h>


static const uint8_t Si446xRadioConfiguration[] PROGMEM = RADIO_CONFIGURATION_DATA_ARRAY;

class LoLaSi446xRadioDriver : public LoLaSi446xSPIDriver
{
private:
	uint8_t InterruptFlags[8];

protected:
	Si446x::Info_t DeviceInfo;

public:
	LoLaSi446xRadioDriver(SPIClass* spi_instance, const uint8_t cs, const uint8_t reset, const uint8_t irq)
		: LoLaSi446xSPIDriver(spi_instance, cs, reset, irq)
	{

		ETTM = 685; // Not Measured with STM32F1 @72 MHz and SPI @ 9 MHz

		//ShortEncode : 0 us
		//LongEncode : 0 us
		//ShortDecode : 0 us
		//LongDecode : 0 us
		//ShortTransmit : 0 us
		//LongTransmit : 0 us
		//ETTM: 0 us
	}

private:
	int16_t rssi_dBm(const uint8_t val)
	{
		return (((int16_t)(val / 2)) - 134);
	}

public:
	/// Driver constants.
	const uint8_t GetTransmitPowerMax()
	{
		return Si446x::SI4463_TRANSMIT_POWER_MAX;
	}

	const uint8_t GetTransmitPowerMin()
	{
		return Si446x::SI4463_TRANSMIT_POWER_MIN;
	}

	const int16_t GetRSSIMax()
	{
		return Si446x::SI4463_RSSI_MAX;
	}

	const int16_t GetRSSIMin()
	{
		return Si446x::SI4463_RSSI_MIN;
	}

	const uint8_t GetChannelMax()
	{
		return Si446x::SI4463_CHANNEL_MAX;
	}

	const uint8_t GetChannelMin()
	{
		return Si446x::SI4463_CHANNEL_MIN;
	}

	virtual const uint8_t GetMaxPacketSize()
	{
		return Si446x::SI4463_PACKET_MAX_SIZE;
	}

protected:
	virtual bool RadioSetup()
	{
		if (!LoLaSi446xSPIDriver::RadioSetup())
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("LoLaSi446xSPIDriver setup failed."));
#endif
		}

		Serial.println(F("RadioSetup"));
		if (!ApplyStartupConfig())
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("ApplyStartupConfig failed."));
#endif
			return false;
		}

		if (!ReadInfo())
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("ReadInfo failed."));
#endif
			return false;
		}

		if (DeviceInfo.PartId != Si446x::PART_NUMBER_SI4463X)
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.print(F("Part number invalid: "));
			Serial.println(DeviceInfo.PartId);
#endif
			return false;
		}

		// Enable packet RX begin and packet sent callbacks.
		if (!SetupCallbacks())
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("ReadInfo failed."));
#endif
			return false;
		}

		if (!SetRadioTransmitPower())
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("SetRadioTransmitPower failed."));
#endif
			return false;
		}

		// Set low battery voltage to 3200mV.
		if (!SetBatteryWarningThreshold(3200))
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("SetRadioTransmitPower failed."));
#endif
			return false;
		}

		// Run check battery every 2 seconds.
		if (!SetupWUT(1, 8192))
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("SetRadioTransmitPower failed."));
#endif
			return false;
		}

		// Soft sleep until start.
		if (!SleepRadio())
		{
#ifdef DEBUG_RADIO_DRIVER
			Serial.println(F("SleepRadio failed."));
#endif
			return false;
		}

		// Start up.
		InterruptsEnabled = false;
		InterruptOn();

		return true;
	}

protected:
	// Easy getters, to keep concerns separated.
	bool HasInterruptSyncDetect()
	{
		return InterruptFlags[4] & Si446x::PENDING_EVENT_SYNC_DETECT;
	}

	bool HasInterruptRX()
	{
		return InterruptFlags[2] & Si446x::PENDING_EVENT_RX;
	}

	bool HasInterruptCRCError()
	{
		return InterruptFlags[2] & Si446x::PENDING_EVENT_CRC_ERROR;
	}

	bool HasInterruptPacketSent()
	{
		return InterruptFlags[2] & Si446x::PENDING_EVENT_SENT_OK;
	}

	bool HasInterruptLowBattery()
	{
		return InterruptFlags[6] & Si446x::PENDING_EVENT_LOW_BATTERY;
	}

	bool HasInterruptWakeUpTimer()
	{
		return InterruptFlags[6] & Si446x::PENDING_EVENT_WUT;
	}

protected:
	// High level communications.
	bool SetupCallbacks()
	{
		//Get properties.
		MessageOut[0] = Si446x::COMMAND_GET_PROPERTY;
		MessageOut[1] = 0;
		MessageOut[2] = 2;
		MessageOut[3] = Si446x::COMMAND_FUNCTION_INFO;
		if (!API(4, 2))
		{
			return false;
		}

		//Set properties message.
		MessageOut[0] = Si446x::COMMAND_SET_PROPERTY;
		MessageOut[1] = (uint8_t)(Si446x::CONFIG_INTERRUPTS_ENABLE >> 8);
		MessageOut[2] = 2;
		MessageOut[3] = (uint8_t)Si446x::CONFIG_INTERRUPTS_ENABLE;
		MessageOut[4] = MessageIn[0] | _BV(5 + 8); // SI446X_CBS_SENT;
		MessageOut[5] = MessageIn[1] | _BV(0); // SI446X_CBS_RXBEGIN;

		//Write properties.
		return API(6, 0);
	}

	// Set new state
	bool SetState(const Si446x::RadioState newState)
	{
		MessageOut[0] = Si446x::COMMAND_SET_STATUS;
		MessageOut[1] = (uint8_t)newState;

		return API(2, 0, RESPONSE_SHORT_TIMEOUT_MICROS);
	}

	// Get current radio state.
	Si446x::RadioState GetState()
	{
		uint8_t state = GetFRR(Si446x::COMMAND_READ_FRR_B);
		if (state == Si446x::RadioState::TX_TUNE)
		{
			state = Si446x::RadioState::TX;
		}
		else if (state == Si446x::RadioState::RX_TUNE)
		{
			state = Si446x::RadioState::RX;
		}
		else if (state == Si446x::RadioState::READY2)
		{
			state = Si446x::RadioState::READY;
		}

		return (Si446x::RadioState)state;
	}

	bool ApplyStartupConfig()
	{
		const uint8_t MaxBufferSize = 17;
		uint8_t ChunckLength = 0;
		for (uint16_t i = 0; i < sizeof(Si446xRadioConfiguration); i++)
		{
			memcpy_P(MessageOut, &Si446xRadioConfiguration[i], MaxBufferSize);

			WaitForResponse(MessageIn, 0, RESPONSE_WAIT_TIMEOUT_MICROS);

			ChunckLength = MessageOut[0];

			RADIO_ATOMIC()
			{
				CHIP_SELECT()
				{
					for (uint8_t i = 0; i < ChunckLength; i++)
					{
						spi->transfer(MessageOut[i + 1]);
					}
				}
			}

			i += ChunckLength;
		}

		return true;
	}

	bool SetProperty(const uint16_t property, const uint8_t value)
	{
		//Prepare property message.
		MessageOut[0] = Si446x::COMMAND_SET_PROPERTY;
		MessageOut[1] = (uint8_t)(property >> 8);
		MessageOut[2] = 1;
		MessageOut[3] = property;
		MessageOut[4] = value;

		//Write property.
		return API(5, 0, RESPONSE_SHORT_TIMEOUT_MICROS);
	}

	bool SetRadioReceiveInternal()
	{
		if (SetState(Si446x::RadioState::SLEEP))
		{
			return false;
		}

		ClearRXFIFO();

		//Start RX.
		MessageOut[0] = Si446x::COMMAND_START_RX;
		MessageOut[1] = GetChannel();
		MessageOut[2] = 0;
		MessageOut[3] = 0;
		MessageOut[4] = 0; //Variable length packets.
		MessageOut[5] = Si446x::RadioState::NOCHANGE;
		MessageOut[6] = Si446x::RadioState::READY;
		MessageOut[7] = Si446x::RadioState::SLEEP;

		return API(8, 0, RESPONSE_SHORT_TIMEOUT_MICROS);
	}

	bool ReadInfo()
	{
		MessageOut[0] = Si446x::COMMAND_GET_PART_INFO;
		if (!API(1, 8))
		{
			Serial.println(F("get part fail"));
			return false;
		}

		DeviceInfo.ChipRevision = MessageIn[0];
		DeviceInfo.PartId = (MessageIn[1] << 8) | MessageIn[2];

		DeviceInfo.PartBuild = MessageIn[3];
		DeviceInfo.DeviceId = (MessageIn[4] << 8) | MessageIn[5];
		DeviceInfo.Customer = MessageIn[6];
		DeviceInfo.ROM = MessageIn[7];

		MessageOut[0] = Si446x::COMMAND_FUNCTION_INFO;
		if (!API(1, 6))
		{
			return false;
		}

		DeviceInfo.RevisionExternal = MessageIn[0];
		DeviceInfo.RevisionBranch = MessageIn[1];
		DeviceInfo.RevisionInternal = MessageIn[2];
		DeviceInfo.Patch = (MessageIn[3] << 8) | MessageIn[4];
		DeviceInfo.Function = MessageIn[5];

		return true;
	}

	void ClearRXFIFO()
	{
		MessageOut[0] = Si446x::COMMAND_FIFO_INFO;
		MessageOut[1] = Si446x::COMMAND_CLEAR_RX_FIFO;

		API(2, 0, RESPONSE_SHORT_TIMEOUT_MICROS);
	}

	void ClearTXFIFO()
	{
		MessageOut[0] = Si446x::COMMAND_FIFO_INFO;
		MessageOut[1] = Si446x::COMMAND_CLEAR_TX_FIFO;

		API(2, 0, RESPONSE_SHORT_TIMEOUT_MICROS);
	}

	uint8_t GetReceivedLength()
	{
		uint8_t Length = 0;

		CHIP_SELECT()
		{
			spi->transfer(Si446x::COMMAND_READ_RX_FIFO);
			Length = spi->transfer(0);
		}

		return Length;
	}

	void ReadReceived(const uint8_t length)
	{
		RADIO_ATOMIC()
		{
			CHIP_SELECT()
			{
#ifdef RADIO_USE_DMA
				spi->dmaTransfer(0, IncomingPacket.GetRaw(), length);
#else
				for (uint8_t i = 0; i < length; i++)
				{
					IncomingPacket.GetRaw()[i] = spi->transfer(0);
				}
#endif
			}
		}
	}

	bool RadioTransmit()
	{
#ifdef LOLA_MOCK_RADIO
		delayMicroseconds(500);
		return true;
#else
		if (!SetState(Si446x::RadioState::SLEEP))
		{
			return false;
		}

		uint8_t OutgoingSize = OutgoingPacket.GetDefinition()->GetTotalSize();

		ClearTXFIFO();

		RADIO_ATOMIC()
		{
			CHIP_SELECT()
			{
				spi->transfer(Si446x::COMMAND_WRITE_TX_FIFO);

#ifdef RADIO_USE_DMA
				spi->dmaSend(OutgoingPacket.GetRaw(), OutgoingSize);
#else
				for (uint8_t i = 0; i < OutgoingSize; i++)
				{
					spi->transfer(OutgoingPacket.GetRaw()[i]);
				}
#endif
			}
		}

		// Set packet length.
		bool Success = SetProperty(Si446x::CONFIG_PACKET_SIZE_ENABLE, OutgoingSize);

		// Start TX.
		MessageOut[0] = Si446x::COMMAND_START_TX;
		MessageOut[1] = GetChannel();
		MessageOut[2] = Si446x::RadioState::SPI_ACTIVE << 4; //On transmitted interrupt.
		MessageOut[3] = 0;
		MessageOut[4] = 0;//Variable length packets.
		MessageOut[5] = 0;
		MessageOut[6] = 0;

		if (Success)
		{
			Success &= API(7, 0, RESPONSE_SHORT_TIMEOUT_MICROS);
		}

		// Reset packet length back to max for receive mode.
		SetProperty(Si446x::CONFIG_PACKET_SIZE_ENABLE, LOLA_PACKET_MAX_PACKET_SIZE);

		return Success;
#endif
	}

	bool SetupWUT(const uint8_t r, const uint16_t m)
	{
		// Maximum value of r is 20
		// The API docs say that if r or m are 0, then they will have the same effect as if they were 1, but this doesn't seem to be the case?

		// Disable WUT.
		// Setup WUT interrupts.
		// Set WUT clock source to internal 32KHz RC.
		if (!SetProperty(Si446x::CONFIG_WUT, 0) ||
			!SetProperty(Si446x::CONFIG_INTERRUPTS_CONTROL, Si446x::COMMAND_LOW_BAT_INTERRUPT_ENABLE) ||
			!SetProperty(Si446x::CONFIG_CLOCK, Si446x::CLOCK_DIVISOR_SELECT_RC))
		{
			return false;
		}

		delayMicroseconds(300); // Need to wait 300us for clock source to stabilize, see GLOBAL_WUT_CONFIG:WUT_EN info

		// Prepare property message.
		MessageOut[0] = Si446x::WUT_LOW_BATTERY_INTERRUPT_ENABLE;
		MessageOut[0] |= 1 << Si446x::WUT_LOW_BATTERY_ENABLE;
		MessageOut[0] |= 1 << Si446x::WUT_ENABLE;
		MessageOut[1] = m >> 8;
		MessageOut[2] = m;
		MessageOut[3] = r | Si446x::LDC_MAX_PERIODS_TWO | (1 << Si446x::WUT_SLEEP);
		MessageOut[4] = 0; // LDC

		// Write property.
		return API(5, 0);
	}

	// Read a fast response register.
	uint8_t GetFRR(const uint8_t reg)
	{
		MessageIn[0] = 0;

		RADIO_ATOMIC()
		{
			CHIP_SELECT()
			{
				spi->transfer(reg);
				MessageIn[0] = spi->transfer(0xFF);
			}
		}
		return MessageIn[0];
	}

	bool SetRadioTransmitPower()
	{
		return SetProperty(Si446x::CONFIG_PA_POWER, GetTransmitPower());
	}

	bool ReadInterruptFlags()
	{
		if (WaitForResponse(MessageIn, 0, RESPONSE_SHORT_TIMEOUT_MICROS)) // Make sure it's ok to send a command
		{
			RADIO_ATOMIC()
			{
				CHIP_SELECT()
				{
					MessageOut[0] = Si446x::COMMAND_GET_STATUS;
					spi->transfer(MessageOut[0]);
				}
			}

			return WaitForResponse(InterruptFlags, sizeof(InterruptFlags), RESPONSE_LONG_TIMEOUT_MICROS);
		}
	}

	// Voltage should be between 1500 and 3050 (mV).
	bool SetBatteryWarningThreshold(const uint16_t voltage)
	{
		//((voltage * 2) - 3000) / 100;
		return SetProperty(Si446x::CONFIG_LOW_BATTERY_THRESHOLD, (voltage / 50) - 30);
	}

	// Get the latched RSSI from the beginning of the packet
	int16_t GetLatchedRSSI()
	{
		return rssi_dBm(GetFRR(Si446x::COMMAND_READ_FRR_A));
	}

	bool SleepRadio()
	{
		if (GetState() == Si446x::RadioState::TX)
		{
			return false;
		}
		else
		{
			return SetState(Si446x::RadioState::SLEEP);
		}
	}
};

#endif

