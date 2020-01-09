// Si446x.h

#ifndef _SI446X_h
#define _SI446X_h

class Si446x
{
public:
	// Data structure for storing chip info.
	struct Info_t {
		uint8_t ChipRevision;
		uint16_t PartId;
		uint8_t PartBuild;
		uint16_t DeviceId;
		uint8_t Customer;
		uint8_t ROM; // ROM ID (3 = revB1B, 6 = revC2A)

		uint8_t RevisionExternal;
		uint8_t RevisionBranch;
		uint8_t RevisionInternal;
		uint16_t Patch;
		uint8_t Function;
	};

	enum RadioState : uint8_t
	{
		NOCHANGE = 0x00,
		SLEEP = 0x01, ///< This will never be returned since SPI activity will wake the radio into ::SI446X_SPI_ACTIVE
		SPI_ACTIVE = 0x02,
		READY = 0x03,
		READY2 = 0x04, ///< Will return as ::SI446X_READY
		TX_TUNE = 0x05, ///< Will return as ::SI446X_TX
		RX_TUNE = 0x06, ///< Will return as ::SI446X_RX
		TX = 0x07,
		RX = 0x08
	};

	// Expected part number.
	static const uint16_t PART_NUMBER_SI4463X = 17507;

	// Channel to listen to (0 - 26).
	static const uint8_t SI4463_CHANNEL_MIN = 0;
	static const uint8_t SI4463_CHANNEL_MAX = 26;

	static const uint8_t SI4463_PACKET_MAX_SIZE = 64;

	// Power range.
	//   0 = -32dBm	(<1uW)
	//   7 =  0dBm	(1mW)
	//  12 =  5dBm	(3.2mW)
	//  22 =  10dBm	(10mW)
	//  40 =  15dBm	(32mW)
	// 100 = 20dBm	(100mW) Requires Dual Antennae
	// 127 = ABSOLUTE_MAX
	static const uint8_t SI4463_TRANSMIT_POWER_MIN = 1;
	static const uint8_t SI4463_TRANSMIT_POWER_MAX = 40;

	// Received RSSI range.
	static const int16_t SI4463_RSSI_MIN = -110;
	static const int16_t SI4463_RSSI_MAX = -80;

	// SPI Communications constants.
	static const uint16_t PROPERTY_GROUP_PA = (0x22 << 8);
	static const uint16_t PROPERTY_GROUP_INTERRUPTS = (0x01 << 8);
	static const uint16_t PROPERTY_GROUP_PACKET = (0x12 << 8);
	static const uint16_t PROPERTY_GROUP_GLOBAL = (0x00 << 8);

	static const uint16_t CONFIG_PA_POWER = PROPERTY_GROUP_PA | 0x01;

	static const uint16_t CONFIG_INTERRUPTS_ENABLE = PROPERTY_GROUP_INTERRUPTS | 0x01;
	static const uint16_t CONFIG_INTERRUPTS_CONTROL = PROPERTY_GROUP_INTERRUPTS | 0x03;

	static const uint16_t CONFIG_PACKET_SIZE_ENABLE = PROPERTY_GROUP_PACKET | 0x12;

	static const uint16_t CONFIG_WUT = PROPERTY_GROUP_GLOBAL | 0x04;
	static const uint16_t CONFIG_CLOCK = PROPERTY_GROUP_GLOBAL | 0x01;
	static const uint16_t CONFIG_LOW_BATTERY_THRESHOLD = PROPERTY_GROUP_GLOBAL | 0x02;


	static const uint8_t COMMAND_GET_PART_INFO = 0x01;
	static const uint8_t COMMAND_FUNCTION_INFO = 0x10;
	static const uint8_t COMMAND_SET_PROPERTY = 0x11;
	static const uint8_t COMMAND_GET_PROPERTY = 0x12;
	static const uint8_t COMMAND_IR_CAL = 0x17;
	static const uint8_t COMMAND_LOW_BAT_INTERRUPT_ENABLE = 0x01;
	static const uint8_t COMMAND_GET_STATUS = 0x20;
	static const uint8_t COMMAND_SET_STATUS = 0x34;
	static const uint8_t COMMAND_START_TX = 0x31;
	static const uint8_t COMMAND_START_RX = 0x32;

	static const uint8_t COMMAND_FIFO_INFO = 0x15;

	static const uint8_t COMMAND_CLEAR_TX_FIFO = 0x01;
	static const uint8_t COMMAND_CLEAR_RX_FIFO = 0x02;

	static const uint8_t COMMAND_READ_RX_FIFO = 0x77;
	static const uint8_t COMMAND_WRITE_TX_FIFO = 0x66;
	static const uint8_t COMMAND_READ_BUFFER = 0x44;
	static const uint8_t COMMAND_READ_FRR_A = 0x50;
	static const uint8_t COMMAND_READ_FRR_B = 0x51;
	static const uint8_t COMMAND_READ_FRR_C = 0x53;
	static const uint8_t COMMAND_READ_FRR_D = 0x57;

	static const uint8_t PENDING_EVENT_SYNC_DETECT = 1 << 0;
	static const uint8_t PENDING_EVENT_LOW_BATTERY = 1 << 1;
	static const uint8_t PENDING_EVENT_WUT = 1 << 0;
	static const uint8_t PENDING_EVENT_CRC_ERROR = 1 << 3;
	static const uint8_t PENDING_EVENT_RX = 1 << 4;
	static const uint8_t PENDING_EVENT_SENT_OK = 1 << 5;


	static const uint8_t CLOCK_DIVISOR_SELECT_OFF = 0x00;
	static const uint8_t CLOCK_DIVISOR_SELECT_RC = 0x01;
	static const uint8_t CLOCK_DIVISOR_SELECT_CRISTAL = 0x02;

	static const uint16_t WUT_ENABLE = 1;
	static const uint16_t WUT_LOW_BATTERY_ENABLE = 0x02;
	static const uint16_t WUT_LOW_BATTERY_INTERRUPT_ENABLE = 0x40;
	static const uint16_t WUT_SLEEP = 0x05;

	static const uint16_t LDC_MAX_PERIODS_FOREVER = 0 << 6;
	static const uint16_t LDC_MAX_PERIODS_TWO = 1 << 6;
	static const uint16_t LDC_MAX_PERIODS_FOUR = 2 << 6;
	static const uint16_t LDC_MAX_PERIODS_EIGHT = 3 << 6;
};


#endif

