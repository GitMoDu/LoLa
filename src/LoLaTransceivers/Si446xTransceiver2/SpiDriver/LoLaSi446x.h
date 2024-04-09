// LoLaSi446x.h

#ifndef _LOLA_SI446X_h
#define _LOLA_SI446X_h

#include <stdint.h>

/// <summary>
/// Rename Si446x after deprecation of wrapper.
/// </summary>
namespace LoLaSi446x
{
	enum class PART_NUMBER : uint16_t
	{
		SI4463 = 17507,
		SI4461 = 0,//TODO:
		SI4460 = 1,//TODO:
	};

	enum class DEVICE_ID : uint16_t
	{
		SI4463 = 15,
		SI4461 = 0,//TODO:
		SI4460 = 1,//TODO:
	};


	enum class RadioStateEnum : uint8_t
	{
		NO_CHANGE = 0x00,
		SLEEP = 0x01, ///< This will never be returned since SPI activity will wake the radio into ::SPI_ACTIVE
		SPI_ACTIVE = 0x02,
		READY = 0x03,
		READY2 = 0x04, ///< Will return as ::READY
		TX_TUNE = 0x05, ///< Will return as ::TX
		RX_TUNE = 0x06, ///< Will return as ::RX
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

	static constexpr uint8_t PH_FLAG_DEBUG = (uint8_t)INT_CTL_PH::PACKET_RX_EN | (uint8_t)INT_CTL_PH::PACKET_SENT_EN | (uint8_t)INT_CTL_PH::CRC_ERROR_EN;
	static constexpr uint8_t MODEM_FLAG_DEBUG = (uint8_t)INT_CTL_MODEM::SYNC_DETECT_EN;
	static constexpr uint8_t CHIP_FLAG_DEBUG = (uint8_t)INT_CTL_CHIP::LOW_BATT_EN | (uint8_t)INT_CTL_CHIP::CMD_ERROR_EN | (uint8_t)INT_CTL_CHIP::FIFO_UNDERFLOW_OVERFLOW_ERROR_EN;
	
	static constexpr uint8_t PH_FLAG = (uint8_t)INT_CTL_PH::PACKET_RX_EN | (uint8_t)INT_CTL_PH::PACKET_SENT_EN;
	static constexpr uint8_t MODEM_FLAG = 0;
	static constexpr uint8_t CHIP_FLAG = (uint8_t)INT_CTL_CHIP::LOW_BATT_EN;

	//Power range.
	//   0 = -32dBm	(<1uW)
	//   7 =  0dBm	(1mW)
	//  12 =  5dBm	(3.2mW)
	//  22 =  10dBm	(10mW)
	//  40 =  15dBm	(32mW)
	// 100 = 20dBm	(100mW) Requires Dual Antennae
	// 127 = ABSOLUTE_MAX
	static constexpr uint8_t SI4463_TRANSMIT_POWER_MIN = 0;
	static constexpr uint8_t SI4463_TRANSMIT_POWER_MAX = 40;

	//Received RSSI range.
	static constexpr int16_t SI4463_RSSI_MIN = -110;
	static constexpr int16_t SI4463_RSSI_MAX = -80;

	static constexpr int16_t RssiToDbM(const uint8_t rssiRegisterValue)
	{
		return (((int16_t)(rssiRegisterValue / 2)) - 134);
	}

	struct Si446xInfoStruct
	{
		uint8_t ChipRevision = 0;
		uint16_t PartId = 0;
		uint8_t PartBuild = 0;
		uint16_t DeviceId = 0;
		uint8_t Customer = 0;
		uint8_t RomId; // ROM ID (3 = revB1B, 6 = revC2A)

		uint8_t RevisionExternal;
		uint8_t RevisionBranch;
		uint8_t RevisionInternal;
		uint16_t Patch;
		uint8_t Function;
	};

	struct Si446xConfigStruct
	{
		uint8_t RfPowerUp[7];
		uint8_t RfGpioPinConfig[8];
		uint8_t Global[8];
		uint8_t Modem0[16];
		uint8_t Modem1[5];
		uint8_t Modem2[16];
		uint8_t Modem3[14];
		uint8_t Modem4[15];
		uint8_t Modem5[13];
		uint8_t Modem6[6];
		uint8_t Modem7[6];
		uint8_t Modem8[9];
		uint8_t ModemFlt0[16];
		uint8_t ModemFlt1[16];
		uint8_t ModemFlt2[15];
		uint8_t Pa[5];
		uint8_t FrequencyControl0[12];
		uint8_t RfStartRx[8];
		uint8_t RfIrCalibration0[5];
		uint8_t RfIrCaibrationl1[5];
		uint8_t InterruptControl[8];
		uint8_t FastReadRegisterControl[7];
		uint8_t Preamble[5];
		uint8_t Sync[8];
		uint8_t Packet0[14];
		uint8_t Packet1[11];
		uint8_t Modem20[14];
		uint8_t Modem21[15];
		uint8_t Modem22[13];
		uint8_t Modem23[9];
		uint8_t Modem24[12];
		uint8_t Modem25[5];
		uint8_t Modem26[5];
		uint8_t Modem27[5];
		uint8_t ModemFlt20[16];
		uint8_t ModemFlt21[16];
		uint8_t ModemFlt23[15];
		uint8_t Synth[10];
		uint8_t FrequencyControl1[8];
	};

	struct RadioEventsStruct
	{
		uint32_t StartTimestamp = 0;
		uint32_t RxTimestamp = 0;

		bool RxStart = false;
		bool RxReady = false;
		bool RxFail = false;
		bool TxDone = false;
		bool VccWarning = false;
		bool CalibrationPending = false;
		bool Error = false;

		void Clear()
		{
			RxStart = false;
			RxReady = false;
			RxFail = false;
			TxDone = false;
			VccWarning = false;
			CalibrationPending = false;
			Error = false;
		}

		const bool Pending()
		{
			return RxStart
				|| RxReady
				|| RxFail
				|| TxDone
				|| VccWarning
				|| CalibrationPending
				|| Error;
		}

		void SetFrom(const uint8_t source[(uint8_t)GET_INT_STATUS_REPLY::GET_INT_STATUS_REPLY_SIZE], const bool merge = true)
		{
			if (merge)
			{
				RxStart |= source[(uint8_t)GET_INT_STATUS_REPLY::MODEM_PEND] & (uint8_t)MODEM_PEND::SYNC_DETECT_PEND;
				RxReady |= source[(uint8_t)GET_INT_STATUS_REPLY::PH_PEND] & (uint8_t)PH_PEND::PACKET_RX_PEND;
				RxFail |= source[(uint8_t)GET_INT_STATUS_REPLY::PH_PEND] & (uint8_t)PH_PEND::CRC_ERROR_PEND;
				TxDone |= source[(uint8_t)GET_INT_STATUS_REPLY::PH_PEND] & (uint8_t)PH_PEND::PACKET_SENT_PEND;
				VccWarning |= source[(uint8_t)GET_INT_STATUS_REPLY::CHIP_PEND] & (uint8_t)CHIP_PEND::LOW_BATT_PEND;
				CalibrationPending |= source[(uint8_t)GET_INT_STATUS_REPLY::CHIP_PEND] & (uint8_t)CHIP_PEND::CAL_PEND;
				Error |= source[(uint8_t)GET_INT_STATUS_REPLY::CHIP_PEND] & ((uint8_t)CHIP_PEND::CMD_ERROR_PEND | (uint8_t)CHIP_PEND::FIFO_UNDERFLOW_OVERFLOW_ERROR_PEND);
			}
			else
			{
				RxStart = source[(uint8_t)GET_INT_STATUS_REPLY::MODEM_PEND] & (uint8_t)MODEM_PEND::SYNC_DETECT_PEND;
				RxReady = source[(uint8_t)GET_INT_STATUS_REPLY::PH_PEND] & (uint8_t)PH_PEND::PACKET_RX_PEND;
				RxFail = source[(uint8_t)GET_INT_STATUS_REPLY::PH_PEND] & (uint8_t)PH_PEND::CRC_ERROR_PEND;
				TxDone = source[(uint8_t)GET_INT_STATUS_REPLY::PH_PEND] & (uint8_t)PH_PEND::PACKET_SENT_PEND;
				VccWarning = source[(uint8_t)GET_INT_STATUS_REPLY::CHIP_PEND] & (uint8_t)CHIP_PEND::LOW_BATT_PEND;
				CalibrationPending = source[(uint8_t)GET_INT_STATUS_REPLY::CHIP_PEND] & (uint8_t)CHIP_PEND::CAL_PEND;
				Error = source[(uint8_t)GET_INT_STATUS_REPLY::CHIP_PEND] & ((uint8_t)CHIP_PEND::CMD_ERROR_PEND | (uint8_t)CHIP_PEND::FIFO_UNDERFLOW_OVERFLOW_ERROR_PEND);
			}

			// Rx Start will be set regardless of interrupt configuration.
			if (RxReady && RxStart)
			{
				RxStart = false;
			}
		}
	};
};
#endif

