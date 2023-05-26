//// LoLaSi446xRadioDriver.h
//
//#ifndef _LOLASI446XRADIODRIVER_h
//#define _LOLASI446XRADIODRIVER_h
//
//
//#define _TASK_OO_CALLBACKS
//#include <TaskSchedulerDeclarations.h>
//
//#include "LoLaSi446xSPIDriver.h"
//
//
//#include "Si446x_config.h"
//#include "Si446x_defs.h"
//
//
//class LoLaSi446xRadioDriver : protected Task
//{
//protected:
//	LoLaSi446xSPIDriver Driver;
//
//	static const uint8_t MESSAGE_OUT_SIZE = 18; //Max internal sent message length;
//	static const uint8_t MESSAGE_IN_SIZE = 8; //Max internal received message length;
//
//	uint8_t MessageIn[MESSAGE_IN_SIZE];
//	uint8_t MessageOut[MESSAGE_OUT_SIZE];
//
//
//
//	/**
//	* @brief Data structure for storing chip info.
//	*/
//	struct Info_t {
//		uint8_t ChipRevision;
//		uint16_t PartId;
//		uint8_t PartBuild;
//		uint16_t DeviceId;
//		uint8_t Customer;
//		uint8_t ROM; // ROM ID (3 = revB1B, 6 = revC2A)
//
//		uint8_t RevisionExternal;
//		uint8_t RevisionBranch;
//		uint8_t RevisionInternal;
//		uint16_t Patch;
//		uint8_t Function;
//	};
//
//	Info_t DeviceInfo;
//
//	enum SI446xSTATE : uint8_t
//	{
//		NOCHANGE = 0x00,
//		SLEEP = 0x01, ///< This will never be returned since SPI activity will wake the radio into ::SI446X_SPI_ACTIVE
//		SPI_ACTIVE = 0x02,
//		READY = 0x03,
//		READY2 = 0x04, ///< Will return as ::SI446X_READY
//		TX_TUNE = 0x05, ///< Will return as ::SI446X_TX
//		RX_TUNE = 0x06, ///< Will return as ::SI446X_RX
//		TX = 0x07,
//		RX = 0x08
//	};
//
//	//Expected part number.
//	static const uint16_t PART_NUMBER_SI4463 = 17507;
//
//	//Channel to listen to (0 - 26).
//	static const uint8_t SI4463_CHANNEL_MIN = 0;
//	static const uint8_t SI4463_CHANNEL_MAX = 26;
//
//	//Power range.
//	//   0 = -32dBm	(<1uW)
//	//   7 =  0dBm	(1mW)
//	//  12 =  5dBm	(3.2mW)
//	//  22 =  10dBm	(10mW)
//	//  40 =  15dBm	(32mW)
//	// 100 = 20dBm	(100mW) Requires Dual Antennae
//	// 127 = ABSOLUTE_MAX
//	static const uint8_t SI4463_TRANSMIT_POWER_MIN = 1;
//	static const uint8_t SI4463_TRANSMIT_POWER_MAX = 40;
//
//	//Received RSSI range.
//	static const int16_t SI4463_RSSI_MIN = -110;
//	static const int16_t SI4463_RSSI_MAX = -80;
//
//
//
//
//	const uint8_t Configuration[] PROGMEM = RADIO_CONFIGURATION_DATA_ARRAY;
//
//
//public:
//	LoLaSi446xRadioDriver(SPI& spi_instance, const uint8_t cs, const uint8_t reset, const uint8_t irq)
//		: Driver(spi_instance, cs, reset, irq)
//	{
//	}
//
//private:
//	static constexpr int16_t rssi_dBm(const uint8_t val)
//	{
//		return (((int16_t)(val / 2)) - 134);
//	}
//
//protected:
//	const bool Setup()
//	{
//		if (Driver.Setup())
//		{
//			return true;
//		}
//
//		return false;
//	}
//
//protected:
//	enum AsyncApiEnum
//	{
//		None,
//		ReadingInfo,
//	};
//
//	const bool DoBlockingApi(const uint8_t sendSize, const uint8_t receiveSize, const uint32_t timeout = 500)
//	{
//		uint32_t start = micros();
//
//		Driver.Send(MessageOut, sendSize);
//
//		if (receiveSize > 0)
//		{
//			while (!(micros() - start > timeout))
//			{
//				if (Driver.GetResponse(MessageIn, receiveSize))
//				{
//					return true;
//				}
//				else
//				{
//					delayMicroseconds(5);
//				}
//			}
//		}
//		else
//		{
//			return true;
//		}
//	}
//
//
//	// High level communications.
//	void SetupCallbacks()
//	{
//		//Get properties.
//		MessageOut[0] = COMMAND_GET_PROPERTY;
//		MessageOut[1] = 0;
//		MessageOut[2] = 2;
//		MessageOut[3] = COMMAND_FUNCTION_INFO;
//		DoBlockingApi(4, 2);
//
//		//Set properties message.
//		MessageOut[0] = COMMAND_SET_PROPERTY;
//		MessageOut[1] = (uint8_t)(CONFIG_INTERRUPTS_ENABLE >> 8);
//		MessageOut[2] = 2;
//		MessageOut[3] = (uint8_t)CONFIG_INTERRUPTS_ENABLE;
//		MessageOut[4] = MessageIn[0] | _BV(5 + 8); // SI446X_CBS_SENT;
//		MessageOut[5] = MessageIn[1] | _BV(0); // SI446X_CBS_RXBEGIN;
//
//		//Write properties.
//		DoBlockingApi(6, 0);
//	}
//
//	// Set new state
//	bool SetState(const SI446xSTATE newState)
//	{
//		MessageOut[0] = COMMAND_SET_STATUS;
//		MessageOut[1] = (uint8_t)newState;
//
//		return DoBlockingApi(2, 0);
//	}
//
//	/*bool WaitForResponse(uint8_t* target, const uint8_t inLength, const uint32_t timeoutMicros = RESPONSE_TIMEOUT_MICROS)
//	{
//	}*/
//
//
//	// Get current radio state.
//	SI446xSTATE GetState()
//	{
//		uint8_t state = GetFRR(COMMAND_READ_FRR_B);
//		if (state == SI446xSTATE::TX_TUNE)
//		{
//			state = SI446xSTATE::TX;
//		}
//		else if (state == SI446xSTATE::RX_TUNE)
//		{
//			state = SI446xSTATE::RX;
//		}
//		else if (state == SI446xSTATE::READY2)
//		{
//			state = SI446xSTATE::READY;
//		}
//
//		return (SI446xSTATE)state;
//	}
//
//	void ApplyStartupConfig()
//	{
//		for (uint16_t i = 0; i < sizeof(Configuration); i++)
//		{
//			memcpy_P(MessageOut, &Configuration[i], 17);
//
//			uint8_t Length = MessageOut[0];
//
//			if (WaitForResponse(MessageIn, 0)) // Make sure it's ok to send a command
//			{
//				CHIP_SELECT()
//				{
//#ifdef RADIO_USE_DMA
//					spi->dmaSend(&MessageOut[1], Length);
//#else
//					for (uint8_t i = 1; i < Length + 1; i++)
//					{
//						spi->transfer(MessageOut[i]);
//					}
//#endif
//				}
//
//				i += Length;
//			}
//		}
//	}
//
//	bool SetProperty(const uint16_t property, const uint8_t value)
//	{
//		//Prepare property message.
//		MessageOut[0] = COMMAND_SET_PROPERTY;
//		MessageOut[1] = (uint8_t)(property >> 8);
//		MessageOut[2] = 1;
//		MessageOut[3] = property;
//		MessageOut[4] = value;
//
//		//Write property.
//		return API(5, 0);
//	}
//
//	bool SetRadioReceiveInternal()
//	{
//		if (SetState(SI446xSTATE::SLEEP))
//		{
//			return false;
//		}
//
//		ClearRXFIFO();
//
//		//Start RX.
//		MessageOut[0] = COMMAND_START_RX;
//		MessageOut[1] = GetChannel();
//		MessageOut[2] = 0;
//		MessageOut[3] = 0;
//		MessageOut[4] = 0; //Variable length packets.
//		MessageOut[5] = SI446xSTATE::NOCHANGE;
//		MessageOut[6] = SI446xSTATE::READY;
//		MessageOut[7] = SI446xSTATE::SLEEP;
//
//		return DoBlockingApi(8, 0);
//	}
//
//	void ReadInfo()
//	{
//		MessageOut[0] = COMMAND_GET_PART_INFO;
//		API(1, 8);
//
//		DeviceInfo.ChipRevision = MessageIn[0];
//		DeviceInfo.PartId = (MessageIn[1] << 8) | MessageIn[2];
//
//		DeviceInfo.PartBuild = MessageIn[3];
//		DeviceInfo.DeviceId = (MessageIn[4] << 8) | MessageIn[5];
//		DeviceInfo.Customer = MessageIn[6];
//		DeviceInfo.ROM = MessageIn[7];
//
//
//		MessageOut[0] = COMMAND_FUNCTION_INFO;
//		API(1, 6);
//
//		DeviceInfo.RevisionExternal = MessageIn[0];
//		DeviceInfo.RevisionBranch = MessageIn[1];
//		DeviceInfo.RevisionInternal = MessageIn[2];
//		DeviceInfo.Patch = (MessageIn[3] << 8) | MessageIn[4];
//		DeviceInfo.Function = MessageIn[5];
//	}
//
//	void ClearRXFIFO()
//	{
//		MessageOut[0] = COMMAND_FIFO_INFO;
//		MessageOut[1] = COMMAND_CLEAR_RX_FIFO;
//
//		API(2, 0);
//	}
//
//	void ClearTXFIFO()
//	{
//		MessageOut[0] = COMMAND_FIFO_INFO;
//		MessageOut[1] = COMMAND_CLEAR_TX_FIFO;
//
//		API(2, 0);
//	}
//
//	uint8_t GetReceivedLength()
//	{
//		uint8_t Length = 0;
//
//		CHIP_SELECT()
//		{
//			spi->transfer(COMMAND_READ_RX_FIFO);
//			Length = spi->transfer(0);
//		}
//
//		return Length;
//	}
//
//	void ReadReceived(const uint8_t length)
//	{
//		RADIO_ATOMIC()
//		{
//			CHIP_SELECT()
//			{
//
//#ifdef RADIO_USE_DMA
//				spi->dmaTransfer(0, IncomingPacket.GetRaw(), length);
//#else
//				for (uint8_t i = 0; i < length; i++)
//				{
//					IncomingPacket.GetRaw()[i] = spi->transfer(0);
//				}
//#endif
//			}
//		}
//	}
//
//	bool RadioTransmitInternal()
//	{
//		if (SetState(SI446xSTATE::SLEEP))
//		{
//			return false;
//		}
//
//		uint8_t OutgoingSize = OutgoingPacket.GetDefinition()->GetTotalSize();
//
//		ClearTXFIFO();
//
//		RADIO_ATOMIC()
//		{
//			CHIP_SELECT()
//			{
//				spi->transfer(COMMAND_WRITE_TX_FIFO);
//#ifdef RADIO_USE_DMA
//				spi->dmaSend(OutgoingPacket.GetRaw(), OutgoingSize);
//#else
//				for (uint8_t i = 0; i < OutgoingSize; i++)
//				{
//					spi->transfer(OutgoingPacket.GetRaw()[i]);
//				}
//#endif
//			}
//		}
//
//		// Set packet length.
//		bool Success = SetProperty(CONFIG_PACKET_SIZE_ENABLE, OutgoingSize);
//
//		// Start TX.
//		MessageOut[0] = COMMAND_START_TX;
//		MessageOut[1] = GetChannel();
//		MessageOut[2] = SI446xSTATE::SPI_ACTIVE << 4; //On transmitted interrupt.
//		MessageOut[3] = 0;
//		MessageOut[4] = 0;//Variable length packets.
//		MessageOut[5] = 0;
//		MessageOut[6] = 0;
//
//		if (Success)
//		{
//			Success &= API(7, 0);
//		}
//
//		// Reset packet length back to max for receive mode.
//		SetProperty(CONFIG_PACKET_SIZE_ENABLE, LOLA_PACKET_MAX_PACKET_SIZE);
//
//		return Success;
//	}
//
//	void SetupWUT(const uint8_t r, const uint16_t m)
//	{
//		//TODO: Complete
//		// Maximum value of r is 20
//		// The API docs say that if r or m are 0, then they will have the same effect as if they were 1, but this doesn't seem to be the case?
//
//		// Disable WUT.
//		SetProperty(CONFIG_WUT, 0);
//
//		// Setup WUT interrupts.
//		SetProperty(CONFIG_INTERRUPTS_CONTROL, COMMAND_LOW_BAT_INTERRUPT_ENABLE);
//
//		// Set WUT clock source to internal 32KHz RC.
//		//SetProperty(SI446X_GLOBAL_CLK_CFG, SI446X_DIVIDED_CLK_32K_SEL_RC);
//
//		//delayMicroseconds(300); // Need to wait 300us for clock source to stabilize, see GLOBAL_WUT_CONFIG:WUT_EN info
//
//		// Prepare property message.
//		//MessageOut[0] = COMMAND_SET_PROPERTY;
//		//MessageOut[0] |= (uint8_t)(CONFIG_WUT >> 8);
//		//MessageOut[2] = 5;
//		//MessageOut[3] = CONFIG_WUT;
//
//		//MessageOut[4] = 1 << CONFIG_WUT_LBD_EN;
//		//MessageOut[5] = m >> 8;
//		//MessageOut[6] = m;
//		//MessageOut[7] = r | SI446X_LDC_MAX_PERIODS_TWO | (1 << SI446X_WUT_SLEEP);
//		//MessageOut[8] = 0;
//
//		// Write property.
//		//API(5, 0);
//	}
//
//	// Read a fast response register.
//	uint8_t GetFRR(const uint8_t reg)
//	{
//		MessageIn[0] = 0;
//
//		RADIO_ATOMIC()
//		{
//			CHIP_SELECT()
//			{
//				spi->transfer(reg);
//				MessageIn[0] = spi->transfer(0xFF);
//			}
//		}
//		return MessageIn[0];
//	}
//
//	/*
//	/ voltage should be between 1500 and 3050 (mV).
//	*/
//	void SetBatteryWarningThreshold(const uint16_t voltage)
//	{
//		//TODO: Complete
//
//		//((voltage * 2) - 3000) / 100;
//		//SetProperty(SI446X_GLOBAL_LOW_BATT_THRESH, (voltage / 50) - 30);
//	}
//
//	// Get the latched RSSI from the beginning of the packet
//	int16_t GetLatchedRSSI()
//	{
//		//TODO: Complete
//		return 0;
//		//return rssi_dBm(GetFRR(SI446X_CMD_READ_FRR_A));
//	}
//
//	bool SleepRadio()
//	{
//		if (GetState() == SI446xSTATE::TX)
//		{
//			return false;
//		}
//		else
//		{
//			SetState(SI446xSTATE::SLEEP);
//			return true;
//		}
//	}
//
//	/// Driver constants.
//	const uint8_t GetTransmitPowerMax()
//	{
//		return SI4463_TRANSMIT_POWER_MAX;
//	}
//
//	const uint8_t GetTransmitPowerMin()
//	{
//		return SI4463_TRANSMIT_POWER_MIN;
//	}
//
//	const int16_t GetRSSIMax()
//	{
//		return SI4463_RSSI_MAX;
//	}
//
//	const int16_t GetRSSIMin()
//	{
//		return SI4463_RSSI_MIN;
//	}
//
//	const uint8_t GetChannelMax()
//	{
//		return SI4463_CHANNEL_MAX;
//	}
//
//	const uint8_t GetChannelMin()
//	{
//		return SI4463_CHANNEL_MIN;
//	}
//};
//
//#endif
//
