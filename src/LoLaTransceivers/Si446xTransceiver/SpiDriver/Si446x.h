// Si446x.h

#ifndef _SI446X_h
#define _SI446X_h

#include <stdint.h>

namespace Si446x
{
	/// <summary>
	/// The SPI interface is designed to operate at a maximum of 10 MHz.
	/// </summary>
	static constexpr uint32_t SPI_CLOCK_SPEED = 10000000;

	/// <summary>
	/// The rising edges of SCLK should be aligned with the center of the SDI/SDO data.
	/// </summary>
	static constexpr int SPI_MODE = SPI_MODE0;
	static constexpr BitOrder SPI_ORDER = MSBFIRST;

	//Power range.
	//   0 = -32dBm	(<1uW)
	//   7 =  0dBm	(1mW)
	//  12 =  5dBm	(3.2mW)
	//  22 =  10dBm	(10mW)
	//  40 =  15dBm	(32mW)
	// 100 = 20dBm	(100mW) Requires Dual Antennae
	// 127 = ABSOLUTE_MAX
	static constexpr uint8_t TRANSMIT_POWER_MAX = 127;

	enum class PART_NUMBER : uint16_t
	{
		SI4463 = 17507,
		SI4461 = 0,	//TODO:
		SI4460 = 1,	//TODO:
		SI4468 = 2	//TODO:
	};

	enum class DEVICE_ID : uint8_t
	{
		SI4463 = 15,
		SI4461 = 0,	//TODO:
		SI4460 = 1,	//TODO:
		SI4468 = 2	//TODO:
	};

	enum class ROM_ID : uint8_t
	{
		SI4463 = 3,
		SI4461 = 0,	//TODO:
		SI4460 = 1,	//TODO:
		SI4468 = 2	//TODO:
	};

	enum class RadioStateEnum : uint8_t
	{
		SLEEP = 0x01, // This will never be returned since SPI activity will wake the radio into ::SPI_ACTIVE
		SPI_ACTIVE = 0x02,
		READY = 0x03,
		READY2 = 0x04,
		TX_TUNE = 0x05,
		RX_TUNE = 0x06,
		TX = 0x07,
		RX = 0x08,
		STATE_COUNT
	};

	/// <summary>
	/// Command values, as named and described by AN625 Si446X API DESCRIPTIONS.
	/// </summary>
	enum class Command : uint8_t
	{
		NO_OP = 0x00,				// No operation command.
		POWER_UP = 0x02,			// Power-up device and mode selection. Modes include operational function.

		PART_INFO = 0x01,			// Reports basic information about the device.
		FUNC_INFO = 0x10,			// Returns the Function revision information of the device.

		SET_PROPERTY = 0x11,		// Sets the value of a property.
		GET_PROPERTY = 0x12,		// Retrieve a property's value.

		GPIO_PIN_CFG = 0x13,		// Configures the GPIO pins.
		GET_ADC_READING = 0x14,		// Retrieve the results of possible ADC conversions.

		FIFO_INFO = 0x15,			// Provides access to transmit and receive fifo counts and reset.
		PACKET_INFO = 0x16,			// Returns information about the last packet received and optionally overrides field length.

		IR_CAL = 0x17,				// Calibrate Image Rejection.

		PROTOCOL_CFG = 0x18,		// Sets the chip up for specified protocol.

		GET_INT_STATUS = 0x20,		// Returns the interrupt status byte.
		GET_PH_STATUS = 0x21,		// Returns the packet handler status.
		GET_MODEM_STATUS = 0x22,	// Returns the modem status byte.
		GET_CHIP_STATUS = 0x23,		// Returns the chip status.

		START_TX = 0x31,			// Switches to TX state and starts packet transmission.
		START_RX = 0x32,			// Switches to RX state.

		REQUEST_DEVICE_STATE = 0x33,// Request current device state.
		CHANGE_STATE = 0x34,		// Update state machine entries.

		READ_CMD_BUFF = 0x44,		// Used to read CTS and the command response.

		FRR_A_READ = 0x50,			// Reads the fast response registers starting with A.
		FRR_B_READ = 0x51,			// Reads the fast response registers starting with B.
		FRR_C_READ = 0x53,			// Reads the fast response registers starting with C.
		FRR_D_READ = 0x57,			// Reads the fast response registers starting with D.

		WRITE_TX_FIFO = 0x66,		// Writes the TX FIFO.
		READ_RX_FIFO = 0x77,		// Reads the RX FIFO.
		RX_HOP = 0x36				// Fast RX to RX transitions for use in frequency hopping systems
	};

	/// <summary>
	/// Property values, as named and described by AN625 Si446X API DESCRIPTIONS.
	/// Does not include all values.
	/// </summary>
	enum class Property : uint16_t
	{
		GLOBAL_XO_TUNE = 0x0000,				// Configure crystal oscillator frequency tuning bank.
		GLOBAL_WUT_CONFIG = 0x0004,				// GLOBAL WUT configuation.
		PKT_FIELD_2_LENGTH_12_8 = 0x1211,		// Byte 1 of field length.
		PKT_FIELD_2_LENGTH_7_0 = 0x1212,		// Byte 0 of field length.
		INT_CTL_ENABLE = 0x0100,				// Interrupt enable property.
		INT_CTL_PH_ENABLE = 0x0101,				// Packet handler interrupt enable property.
		INT_CTL_MODEM_ENABLE = 0x0102,			// Modem interrupt enable property.
		INT_CTL_CHIP_ENABLE = 0x0103,			// Chip interrupt enable property.
		MODEM_MOD_TYPE = 0x2000,				// Modulation Type.
		PA_PWR_LVL = 0x2201,					// PA Level Configuration.
	};

	enum class FIFO_INFO_PROPERY : uint8_t
	{
		CLEAR_TX = 1 << 0,
		CLEAR_RX = 1 << 1,
		CLEAR_RX_TX = CLEAR_TX | CLEAR_RX
	};

	enum class INT_CTL_PH : uint8_t
	{
		RX_FIFO_ALMOST_FULL_EN = 1 << 0,
		TX_FIFO_ALMOST_EMPTY_EN = 1 << 1,
		CRC_ERROR_EN = 1 << 3,
		PACKET_RX_EN = 1 << 4,
		PACKET_SENT_EN = 1 << 5,
		FILTER_MISS_EN = 1 << 6,
		FILTER_MATCH_EN = 1 << 7
	};

	enum class INT_CTL_MODEM : uint8_t
	{
		SYNC_DETECT_EN = 1 << 0,
		PREAMBLE_DETECT_EN = 1 << 1,
		INVALID_PREAMBLE_EN = 1 << 2,
		RSSI_EN = 1 << 3,
		RSSI_JUMP_EN = 1 << 4,
		INVALID_SYNC_EN = 1 << 5
	};

	enum class INT_CTL_CHIP : uint8_t
	{
		WUT_EN = 1 << 0,
		LOW_BATT_EN = 1 << 1,
		CHIP_READY_EN = 1 << 2,
		CMD_ERROR_EN = 1 << 3,
		STATE_CHANGE_EN = 1 << 4,
		FIFO_UNDERFLOW_OVERFLOW_ERROR_EN = 1 << 5
	};

	enum class GET_INT_STATUS_REPLY : uint8_t
	{
		INT_PEND = 0,
		INT_STATUS = 1,
		PH_PEND = 2,
		PH_STATUS = 3,
		MODEM_PEND = 4,
		MODEM_STATUS = 5,
		CHIP_PEND = 6,
		CHIP_STATUS = 7,
		GET_INT_STATUS_REPLY_SIZE
	};

	enum class INT_PEND : uint8_t
	{
		PH_INT_STATUS_PEND = 1 << 0,
		MODEM_INT_STATUS_PEND = 1 << 1,
		CHIP_INT_STATUS_PEND = 1 << 2
	};

	enum class INT_STATUS : uint8_t
	{
		PH_INT_STATUS = 1 << 0,
		MODEM_INT_STATUS = 1 << 1,
		CHIP_INT_STATUS = 1 << 2
	};

	enum class PH_PEND : uint8_t
	{
		RX_FIFO_ALMOST_FULL_PEND = 1 << 0,
		TX_FIFO_ALMOST_EMPTY_PEND = 1 << 1,
		CRC_ERROR_PEND = 1 << 3,
		PACKET_RX_PEND = 1 << 4,
		PACKET_SENT_PEND = 1 << 5,
		FILTER_MISS_PEND = 1 << 6,
		FILTER_MATCH_PEND = 1 << 7
	};

	enum class PH_STATUS : uint8_t
	{
		RX_FIFO_ALMOST_FULL = 1 << 0,
		TX_FIFO_ALMOST_EMPTY = 1 << 1,
		CRC_ERROR = 1 << 3,
		PACKET_RX = 1 << 4,
		PACKET_SENT = 1 << 5,
		FILTER_MISS = 1 << 6,
		FILTER_MATCH = 1 << 7
	};

	enum class MODEM_PEND : uint8_t
	{
		SYNC_DETECT_PEND = 1 << 0,
		PREAMBLE_DETECT_PEND = 1 << 1,
		INVALID_PREAMBLE_PEND = 1 << 2,
		RSSI_PEND = 1 << 3,
		RSSI_JUMP_PEND = 1 << 4,
		INVALID_SYNC_PEND = 1 << 5
	};

	enum class MODEM_STATUS : uint8_t
	{
		SYNC_DETECT = 1 << 0,
		PREAMBLE_DETECT = 1 << 1,
		INVALID_PREAMBLE = 1 << 2,
		RSSI = 1 << 3,
		RSSI_JUMP = 1 << 4,
		INVALID_SYNC = 1 << 5
	};

	enum class CHIP_PEND : uint8_t
	{
		WUT_PEND = 1 << 0,
		LOW_BATT_PEND = 1 << 1,
		CHIP_READY_PEND = 1 << 2,
		CMD_ERROR_PEND = 1 << 3,
		STATE_CHANGE_PEND = 1 << 4,
		FIFO_UNDERFLOW_OVERFLOW_ERROR_PEND = 1 << 5,
		CAL_PEND = 1 << 6
	};

	enum class CHIP_STATUS : uint8_t
	{
		WUT = 1 << 0,
		LOW_BATT = 1 << 1,
		CHIP_READY = 1 << 2,
		CMD_ERROR = 1 << 3,
		STATE_CHANGE = 1 << 4,
		FIFO_UNDERFLOW_OVERFLOW_ERROR = 1 << 5,
		CAL = 1 << 6
	};

	struct PartInfoStruct
	{
		uint16_t PartId = 0;
		uint16_t DeviceId = 0;
		uint8_t ChipRevision = 0;
		uint8_t PartBuild = 0;
		uint8_t Customer = 0;
		uint8_t RomId = 0; // ROM ID (3 = revB1B, 6 = revC2A)

		void SetFrom(const uint8_t source[8])
		{
			ChipRevision = source[0];
			PartId = ((uint16_t)source[1] << 8) | source[2];
			PartBuild = source[3];
			DeviceId = ((uint16_t)source[4] << 8) | source[5];
			Customer = source[6];
			RomId = source[7];
		}
	};

	struct FunctionInfoStruct
	{
		uint16_t Patch;
		uint8_t RevisionExternal;
		uint8_t RevisionBranch;
		uint8_t RevisionInternal;
		uint8_t Function;

		void SetFrom(const uint8_t source[6])
		{
			RevisionExternal = source[0];
			RevisionBranch = source[1];
			RevisionInternal = source[2];
			Patch = ((uint16_t)source[3] << 8) | source[4];
			Function = source[5];
		}
	};

	struct PacketHandlerInterrupts
	{
		static constexpr bool RxFifoAlmostFull(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)PH_STATUS::RX_FIFO_ALMOST_FULL;
		}

		static constexpr bool TxFifoAlmostEmpty(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)PH_STATUS::TX_FIFO_ALMOST_EMPTY;
		}

		static constexpr bool CrcError(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)PH_STATUS::CRC_ERROR;
		}

		static constexpr bool PacketReceived(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)PH_STATUS::PACKET_RX;
		}

		static constexpr bool PacketSent(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)PH_STATUS::PACKET_SENT;
		}

		static constexpr bool FilterMiss(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)PH_STATUS::FILTER_MISS;
		}

		static constexpr bool FilterMatch(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)PH_STATUS::FILTER_MATCH;
		}
	};

	struct ModemInterrupts
	{
		static constexpr bool SyncDetected(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)MODEM_PEND::SYNC_DETECT_PEND;
		}

		static constexpr bool InvalidPreamble(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)MODEM_PEND::INVALID_PREAMBLE_PEND;
		}

		static constexpr bool RssiUpdated(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)MODEM_PEND::RSSI_PEND;
		}

		static constexpr bool RssiJump(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)MODEM_PEND::RSSI_JUMP_PEND;
		}

		static constexpr bool InvalidSync(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)MODEM_PEND::INVALID_SYNC_PEND;
		}
	};

	struct ChipInterrupts
	{
		static constexpr bool WakeUpTimer(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)CHIP_PEND::WUT_PEND;
		}

		static constexpr bool LowBattery(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)CHIP_PEND::LOW_BATT_PEND;
		}

		static constexpr bool ChipReady(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)CHIP_PEND::CHIP_READY_PEND;
		}

		static constexpr bool CommandError(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)CHIP_PEND::CMD_ERROR_PEND;
		}

		static constexpr bool StateChange(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)CHIP_PEND::STATE_CHANGE_PEND;
		}

		static constexpr bool FifoUnderOverFlowError(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)CHIP_PEND::FIFO_UNDERFLOW_OVERFLOW_ERROR_PEND;
		}

		static constexpr bool CalibrationPending(const uint8_t stateCode)
		{
			return stateCode & (uint8_t)CHIP_PEND::CAL_PEND;
		}
	};

	enum class RadioErrorCodeEnum : uint16_t
	{
		InterruptWhileTx = 1 << 0,
		InterruptWhileRx = 1 << 1,
		InterruptWhileHop = 1 << 2,
		InterruptRxFailed = 1 << 3,
		InterruptTxFailed = 1 << 4,
		RxWhileTx = 1 << 5,
		TxTimeout = 1 << 6,
		RxReadError = 1 << 7,
		TxRestoreError = 1 << 8,
		RxRestoreError = 1 << 9
	};

	static constexpr int16_t RssiToDbM(const uint8_t rssi)
	{
		return (((int16_t)(rssi / 2)) - 134);
	}
};
#endif