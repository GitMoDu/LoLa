// LoLaSi446xRadioDriver.h

#ifndef _LOLASI446XRADIODRIVER_h
#define _LOLASI446XRADIODRIVER_h


#include <PacketDriver\RadioDrivers\LoLaSi446x\LoLaSi446xSPIDriver.h>
#include <PacketDriver\RadioDrivers\LoLaSi446x\radio_config.h>
#include <PacketDriver\RadioDrivers\LoLaSi446x\Si446x.h>

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
	}

private:
	inline int16_t rssi_dBm(const uint8_t val)
	{
		return (((int16_t)(val / 2)) - 134);
	}

protected:
	virtual bool RadioSetup()
	{
		if (LoLaSi446xSPIDriver::RadioSetup())
		{
			Serial.println(F("RadioSetup"));
			ApplyStartupConfig();

			ReadInfo();

			if (DeviceInfo.PartId == Si446x::PART_NUMBER_SI4463X)
			{
				SetupCallbacks(); // Enable packet RX begin and packet sent callbacks.
				SetRadioPower();
				SetBatteryWarningThreshold(3200); // Set low battery voltage to 3200mV.

				SetupWUT(1, 8192); // Run check battery every 2 seconds.

				SleepRadio();

				// Start up.
				InterruptsEnabled = false;
				InterruptOn();

				return true;
			}
#ifdef DEBUG_LOLA
			else
			{
				Serial.print(F("Part number invalid: "));
				Serial.println(DeviceInfo.PartId);
				Serial.println(F("Si4463 Driver failed to start."));
			}
#endif // DEBUG_LOLA
		}

		return false;
	}

protected:

	// High level communications.
	void SetupCallbacks()
	{
		//Get properties.
		MessageOut[0] = Si446x::COMMAND_GET_PROPERTY;
		MessageOut[1] = 0;
		MessageOut[2] = 2;
		MessageOut[3] = Si446x::COMMAND_FUNCTION_INFO;
		API(4, 2);

		//Set properties message.
		MessageOut[0] = Si446x::COMMAND_SET_PROPERTY;
		MessageOut[1] = (uint8_t)(Si446x::CONFIG_INTERRUPTS_ENABLE >> 8);
		MessageOut[2] = 2;
		MessageOut[3] = (uint8_t)Si446x::CONFIG_INTERRUPTS_ENABLE;
		MessageOut[4] = MessageIn[0] | _BV(5 + 8); // SI446X_CBS_SENT;
		MessageOut[5] = MessageIn[1] | _BV(0); // SI446X_CBS_RXBEGIN;

		//Write properties.
		API(6, 0);
	}

	// Set new state
	bool SetState(const Si446x::RadioState newState)
	{
		MessageOut[0] = Si446x::COMMAND_SET_STATUS;
		MessageOut[1] = (uint8_t)newState;

		return API(2, 0);
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

	void ApplyStartupConfig()
	{
		for (uint16_t i = 0; i < sizeof(Si446xRadioConfiguration); i++)
		{
			memcpy_P(MessageOut, &Si446xRadioConfiguration[i], 17);

			uint8_t Length = MessageOut[0];

			if (WaitForResponse(MessageIn, 0)) // Make sure it's ok to send a command
			{
				CHIP_SELECT()
				{
#ifdef RADIO_USE_DMA
					spi->dmaSend(&MessageOut[1], Length);
#else
					for (uint8_t i = 1; i < Length + 1; i++)
					{
						spi->transfer(MessageOut[i]);
					}
#endif
				}

				i += Length;
			}
		}
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
		return API(5, 0);
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

		return API(8, 0);
	}

	void ReadInfo()
	{
		MessageOut[0] = Si446x::COMMAND_GET_PART_INFO;
		API(1, 8);

		DeviceInfo.ChipRevision = MessageIn[0];
		DeviceInfo.PartId = (MessageIn[1] << 8) | MessageIn[2];

		DeviceInfo.PartBuild = MessageIn[3];
		DeviceInfo.DeviceId = (MessageIn[4] << 8) | MessageIn[5];
		DeviceInfo.Customer = MessageIn[6];
		DeviceInfo.ROM = MessageIn[7];


		MessageOut[0] = Si446x::COMMAND_FUNCTION_INFO;
		API(1, 6);

		DeviceInfo.RevisionExternal = MessageIn[0];
		DeviceInfo.RevisionBranch = MessageIn[1];
		DeviceInfo.RevisionInternal = MessageIn[2];
		DeviceInfo.Patch = (MessageIn[3] << 8) | MessageIn[4];
		DeviceInfo.Function = MessageIn[5];
	}

	void ClearRXFIFO()
	{
		MessageOut[0] = Si446x::COMMAND_FIFO_INFO;
		MessageOut[1] = Si446x::COMMAND_CLEAR_RX_FIFO;

		API(2, 0);
	}

	void ClearTXFIFO()
	{
		MessageOut[0] = Si446x::COMMAND_FIFO_INFO;
		MessageOut[1] = Si446x::COMMAND_CLEAR_TX_FIFO;

		API(2, 0);
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

	bool RadioTransmitInternal()
	{
		if (SetState(Si446x::RadioState::SLEEP))
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
			Success &= API(7, 0);
		}

		// Reset packet length back to max for receive mode.
		SetProperty(Si446x::CONFIG_PACKET_SIZE_ENABLE, LOLA_PACKET_MAX_PACKET_SIZE);

		return Success;
	}

	void SetupWUT(const uint8_t r, const uint16_t m)
	{
		//TODO: Complete
		// Maximum value of r is 20
		// The API docs say that if r or m are 0, then they will have the same effect as if they were 1, but this doesn't seem to be the case?

		// Disable WUT.
		SetProperty(Si446x::CONFIG_WUT, 0);

		// Setup WUT interrupts.
		SetProperty(Si446x::CONFIG_INTERRUPTS_CONTROL, Si446x::COMMAND_LOW_BAT_INTERRUPT_ENABLE);

		// Set WUT clock source to internal 32KHz RC.
		//SetProperty(SI446X_GLOBAL_CLK_CFG, SI446X_DIVIDED_CLK_32K_SEL_RC);

		//delayMicroseconds(300); // Need to wait 300us for clock source to stabilize, see GLOBAL_WUT_CONFIG:WUT_EN info

		// Prepare property message.
		//MessageOut[0] = COMMAND_SET_PROPERTY;
		//MessageOut[0] |= (uint8_t)(CONFIG_WUT >> 8);
		//MessageOut[2] = 5;
		//MessageOut[3] = CONFIG_WUT;

		//MessageOut[4] = 1 << CONFIG_WUT_LBD_EN;
		//MessageOut[5] = m >> 8;
		//MessageOut[6] = m;
		//MessageOut[7] = r | SI446X_LDC_MAX_PERIODS_TWO | (1 << SI446X_WUT_SLEEP);
		//MessageOut[8] = 0;

		// Write property.
		//API(5, 0);
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

	void ReadInterruptFlags()
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

			WaitForResponse(InterruptFlags, sizeof(InterruptFlags), RESPONSE_SHORT_TIMEOUT_MICROS);
		}
	}

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
		return InterruptFlags[6] & (1 << Si446x::PENDING_EVENT_WUT);
	}

	/*
	/ voltage should be between 1500 and 3050 (mV).
	*/
	void SetBatteryWarningThreshold(const uint16_t voltage)
	{
		//TODO: Complete

		//((voltage * 2) - 3000) / 100;
		//SetProperty(SI446X_GLOBAL_LOW_BATT_THRESH, (voltage / 50) - 30);
	}

	// Get the latched RSSI from the beginning of the packet
	int16_t GetLatchedRSSI()
	{
		//TODO: Complete
		return 0;
		//return rssi_dBm(GetFRR(SI446X_CMD_READ_FRR_A));
	}

	bool SleepRadio()
	{
		if (GetState() == Si446x::RadioState::TX)
		{
			return false;
		}
		else
		{
			SetState(Si446x::RadioState::SLEEP);
			return true;
		}
	}

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
};

#endif

