// LoLaSi446x.h

#ifndef _LOLA_SI446X_h
#define _LOLA_SI446X_h

#include <stdint.h>


struct LoLaSi446x
{
public:
	struct RadioEventStruct
	{
		bool RxStart = false;
		bool RxReady = false;
		bool RxFail = false;
		bool TxDone = false;
		bool WakeUp = false;

		void Clear()
		{
			RxStart = false;
			RxReady = false;
			RxFail = false;
			TxDone = false;
			WakeUp = false;
		}
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

public:
	//Expected part number.
	static constexpr uint16_t PART_NUMBER_SI4463 = 17507;
	static constexpr uint16_t PART_NUMBER_SI4461 = 0;
	static constexpr uint16_t PART_NUMBER_SI4460 = 0;

	//Power range.
	//   0 = -32dBm	(<1uW)
	//   7 =  0dBm	(1mW)
	//  12 =  5dBm	(3.2mW)
	//  22 =  10dBm	(10mW)
	//  40 =  15dBm	(32mW)
	// 100 = 20dBm	(100mW) Requires Dual Antennae
	// 127 = ABSOLUTE_MAX
	static constexpr uint8_t SI4463_TRANSMIT_POWER_MIN = 1;
	static constexpr uint8_t SI4463_TRANSMIT_POWER_MAX = 40;

	//Received RSSI range.
	static constexpr int16_t SI4463_RSSI_MIN = -110;
	static constexpr int16_t SI4463_RSSI_MAX = -80;

public:
	const int16_t rssi_dBm(const uint8_t rssiRegisterValue)
	{
		return (((int16_t)(rssiRegisterValue / 2)) - 134);
	}

	//#define SI446X_CMD_POWER_UP				0x02
	//#define SI446X_CMD_NOP					0x00
	//#define SI446X_CMD_PART_INFO			0x01
	//#define SI446X_CMD_FUNC_INFO			0x10
	//#define SI446X_CMD_SET_PROPERTY			0x11
	//#define SI446X_CMD_GET_PROPERTY			0x12
	//#define SI446X_CMD_GPIO_PIN_CFG			0x13
	//#define SI446X_CMD_FIFO_INFO			0x15
	//#define SI446X_CMD_GET_INT_STATUS		0x20
	//#define SI446X_CMD_REQUEST_DEVICE_STATE	0x33
	//#define SI446X_CMD_CHANGE_STATE			0x34
	//#define SI446X_CMD_READ_CMD_BUFF		0x44
	//#define SI446X_CMD_READ_FRR_A			0x50
	//#define SI446X_CMD_READ_FRR_B			0x51
	//#define SI446X_CMD_READ_FRR_C			0x53
	//#define SI446X_CMD_READ_FRR_D			0x57
	//#define SI446X_CMD_IRCAL				0x17
	//#define SI446X_CMD_IRCAL_MANUAL			0x1a
	//#define SI446X_CMD_START_TX				0x31
	//#define SI446X_CMD_TX_HOP				0x37
	//#define SI446X_CMD_WRITE_TX_FIFO		0x66
	//#define SI446X_CMD_PACKET_INFO			0x16
	//#define SI446X_CMD_GET_MODEM_STATUS		0x22
	//#define SI446X_CMD_START_RX				0x32
	//#define SI446X_CMD_RX_HOP				0x36
	//#define SI446X_CMD_READ_RX_FIFO			0x77
	//#define SI446X_CMD_GET_ADC_READING		0x14
	//#define SI446X_CMD_GET_PH_STATUS		0x21
	//#define SI446X_CMD_GET_CHIP_STATUS		0x23
	//
	//#define SI446X_INT_CTL_CHIP_LOW_BATT_EN	1
	//#define SI446X_INT_CTL_CHIP_WUT_EN		0


public:
	static constexpr uint8_t COMMAND_READ_BUFFER = 0x44;
	static constexpr uint16_t PROPERTY_GROUP_PA = (0x22 << 8);
	static constexpr uint16_t PROPERTY_GROUP_INTERRUPTS = (0x01 << 8);
	static constexpr uint16_t PROPERTY_GROUP_PACKET = (0x12 << 8);
	static constexpr uint16_t PROPERTY_GROUP_GLOBAL = (0x00 << 8);

	static constexpr uint16_t CONFIG_PA_POWER = PROPERTY_GROUP_PA | 0x01;
	//static constexpr uint16_t CONFIG_INTERRUPTS_ENABLE = PROPERTY_GROUP_INTERRUPTS | 0x00;
	static constexpr uint16_t CONFIG_INTERRUPTS_PHY_ENABLE = PROPERTY_GROUP_INTERRUPTS | 0x01;
	static constexpr uint16_t CONFIG_INTERRUPTS_MODEM_ENABLE = PROPERTY_GROUP_INTERRUPTS | 0x02;
	static constexpr uint16_t CONFIG_INTERRUPTS_CHIP_ENABLE = PROPERTY_GROUP_INTERRUPTS | 0x03;

	static constexpr uint16_t CONFIG_PACKET_SIZE_SET = PROPERTY_GROUP_PACKET | 0x12;
	static constexpr uint16_t CONFIG_WUT = PROPERTY_GROUP_GLOBAL | 0x04;
	static constexpr uint16_t CONFIG_WUT_LBD_EN = 0x02;

	static constexpr uint8_t COMMAND_GET_PART_INFO = 0x01;
	static constexpr uint8_t COMMAND_FUNCTION_INFO = 0x10;
	static constexpr uint8_t COMMAND_SET_PROPERTY = 0x11;
	static constexpr uint8_t COMMAND_GET_PROPERTY = 0x12;
	static constexpr uint8_t COMMAND_IR_CAL = 0x17;
	static constexpr uint8_t COMMAND_LOW_BAT_INTERRUPT_ENABLE = 0x01;
	static constexpr uint8_t COMMAND_GET_STATUS = 0x20;
	static constexpr uint8_t COMMAND_SET_STATUS = 0x34;
	static constexpr uint8_t COMMAND_START_TX = 0x31;
	static constexpr uint8_t COMMAND_START_RX = 0x32;
	static constexpr uint8_t COMMAND_GET_INTERRUPT_STATUS = 0x20;

	static constexpr uint8_t COMMAND_FIFO_INFO = 0x15;

	static constexpr uint8_t COMMAND_CLEAR_TX_FIFO = 0x01;
	static constexpr uint8_t COMMAND_CLEAR_RX_FIFO = 0x02;

	static constexpr uint8_t COMMAND_READ_RX_FIFO = 0x77;
	static constexpr uint8_t COMMAND_WRITE_TX_FIFO = 0x66;
	static constexpr uint8_t COMMAND_READ_FRR_A = 0x50;
	static constexpr uint8_t COMMAND_READ_FRR_B = 0x51;
	static constexpr uint8_t COMMAND_READ_FRR_C = 0x53;
	static constexpr uint8_t COMMAND_READ_FRR_D = 0x57;

	static constexpr uint8_t PENDING_EVENT_SYNC_DETECT = 1 << 0;
	static constexpr uint8_t PENDING_EVENT_LOW_BATTERY = 1 << 1;
	static constexpr uint8_t PENDING_EVENT_WUT = 1 << 0;
	static constexpr uint8_t PENDING_EVENT_CRC_ERROR = 1 << 3;
	static constexpr uint8_t PENDING_EVENT_RX = 1 << 4;
	static constexpr uint8_t PENDING_EVENT_SENT_OK = 1 << 5;

	static constexpr uint8_t INTERRUPT_SENT = 1 << 5;
	static constexpr uint8_t INTERRUPT_RX_COMPLETE = 1 << 4;
	static constexpr uint8_t INTERRUPT_RX_INVALID = 1 << 3;
	static constexpr uint8_t INTERRUPT_RX_BEGIN = 1 << 0;

public:
	/**
	* @brief Data structure for storing chip info.
	*/
	struct Si446xInfoStruct {
		uint8_t ChipRevision;
		uint16_t PartId;
		uint8_t PartBuild;
		uint16_t DeviceId;
		uint8_t Customer;
		uint8_t RomId; // ROM ID (3 = revB1B, 6 = revC2A)

		uint8_t RevisionExternal;
		uint8_t RevisionBranch;
		uint8_t RevisionInternal;
		uint16_t Patch;
		uint8_t Function;
	};


};
#endif

