// Si4463Transceiver_433_70_250.h

#ifndef _SI4463_TRANSCEIVER_433_70_250_h
#define _SI4463_TRANSCEIVER_433_70_250_h

#include "Abstract/Si446xAbstractTransceiver.h"

namespace Si4463_433_70_250
{
	struct RadioConfig
	{
		static constexpr uint16_t TRANSCEIVER_ID = 0x4463;

		static constexpr uint32_t BaseFrequency = 433050000;
		static constexpr uint32_t BitRate = 250000;

		static constexpr uint8_t ChannelCount = 70;
		static constexpr uint8_t ChannelSeparationMin = 7;
		static constexpr uint8_t ChannelSeparationAbstractMin = 26;

		static constexpr uint16_t RX_OFFSET_LONG = 17;
		static constexpr uint16_t RX_OFFSET_SHORT = 14;

		// TODO: Values change depending on MCU, needs setup-time calibration.
		static constexpr uint16_t TTA_LONG = 191;
		static constexpr uint16_t TTA_SHORT = 169;

		static constexpr uint16_t DIA_LONG = 1651 - TTA_LONG;
		static constexpr uint16_t DIA_SHORT = 816 - TTA_SHORT;

		static constexpr uint8_t RSSI_LATCH_MIN = 60;
		static constexpr uint8_t RSSI_LATCH_MAX = 140;
	};

	static constexpr uint8_t SetupConfig[] = {
		0x07, 0x02, 0x01, 0x00, 0x01, 0xC9, 0xC3, 0x80,
		0x08, 0x13, 0x41, 0x41, 0x21, 0x20, 0x67, 0x4B, 0x00,
		0x08, 0x11, 0x00, 0x04, 0x00, 0x52, 0x00, 0x18, 0x30,
		0x10, 0x11, 0x20, 0x0C, 0x00, 0x03, 0x00, 0x07, 0x02, 0x71, 0x00, 0x05, 0xC9, 0xC3, 0x80, 0x00, 0x00,
		0x05, 0x11, 0x20, 0x01, 0x0C, 0x46,
		0x10, 0x11, 0x20, 0x0C, 0x1C, 0x80, 0x00, 0xB0, 0x10, 0x0C, 0xE8, 0x00, 0x4E, 0x06, 0x8D, 0xB9, 0x00,
		0x0E, 0x11, 0x20, 0x0A, 0x28, 0x00, 0x02, 0xC0, 0x08, 0x00, 0x12, 0x00, 0x23, 0x01, 0x5C,
		0x0F, 0x11, 0x20, 0x0B, 0x39, 0x11, 0x11, 0x80, 0x1A, 0x20, 0x00, 0x00, 0x28, 0x0C, 0xA4, 0x23,
		0x0D, 0x11, 0x20, 0x09, 0x45, 0x03, 0x00, 0x85, 0x01, 0x00, 0xFF, 0x06, 0x09, 0x10,
		0x06, 0x11, 0x20, 0x02, 0x50, 0x94, 0x0A,
		0x06, 0x11, 0x20, 0x02, 0x54, 0x03, 0x07,
		0x09, 0x11, 0x20, 0x05, 0x5B, 0x40, 0x04, 0x04, 0x78, 0x20,
		0x10, 0x11, 0x21, 0x0C, 0x00, 0x7E, 0x64, 0x1B, 0xBA, 0x58, 0x0B, 0xDD, 0xCE, 0xD6, 0xE6, 0xF6, 0x00,
		0x10, 0x11, 0x21, 0x0C, 0x0C, 0x03, 0x03, 0x15, 0xF0, 0x3F, 0x00, 0x7E, 0x64, 0x1B, 0xBA, 0x58, 0x0B,
		0x0F, 0x11, 0x21, 0x0B, 0x18, 0xDD, 0xCE, 0xD6, 0xE6, 0xF6, 0x00, 0x03, 0x03, 0x15, 0xF0, 0x3F,
		0x05, 0x11, 0x22, 0x01, 0x03, 0x1D,
		0x0C, 0x11, 0x40, 0x08, 0x00, 0x37, 0x09, 0x00, 0x00, 0x06, 0xD4, 0x20, 0xFE,
		0x08, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x05, 0x17, 0x56, 0x10, 0xCA, 0xF0,
		0x05, 0x17, 0x13, 0x10, 0xCA, 0xF0,
		0x08, 0x11, 0x01, 0x04, 0x00, 0x07, 0x18, 0x00, 0x00,
		0x07, 0x11, 0x02, 0x03, 0x00, 0x0A, 0x09, 0x00,
		0x09, 0x11, 0x10, 0x05, 0x00, 0x04, 0x14, 0x00, 0x0F, 0x31,
		0x08, 0x11, 0x11, 0x04, 0x01, 0xB4, 0x2B, 0x00, 0x00,
		0x0F, 0x11, 0x12, 0x0B, 0x05, 0x20, 0x10, 0x00, 0x2A, 0x01, 0x00, 0x30, 0x30, 0x00, 0x01, 0x06,
		0x06, 0x11, 0x12, 0x02, 0x12, 0x80, 0x02,
		0x0E, 0x11, 0x20, 0x0A, 0x03, 0x26, 0x25, 0xA0, 0x01, 0xC9, 0xC3, 0x80, 0x00, 0x06, 0xD4,
		0x0F, 0x11, 0x20, 0x0B, 0x1E, 0x00, 0x30, 0x00, 0xE8, 0x00, 0x78, 0x04, 0x44, 0x44, 0x07, 0xFF,
		0x0C, 0x11, 0x20, 0x08, 0x2A, 0x00, 0x00, 0x00, 0x23, 0x88, 0x89, 0x01, 0x04,
		0x0B, 0x11, 0x20, 0x07, 0x38, 0x22, 0x0D, 0x0D, 0x80, 0x1A, 0x0C, 0xCD,
		0x0B, 0x11, 0x20, 0x07, 0x47, 0x23, 0x01, 0x00, 0x80, 0x06, 0x02, 0x18,
		0x05, 0x11, 0x20, 0x01, 0x50, 0x84,
		0x05, 0x11, 0x20, 0x01, 0x5D, 0x01,
		0x10, 0x11, 0x21, 0x0C, 0x00, 0xE7, 0xDF, 0xCA, 0xAA, 0x84, 0x5D, 0x3A, 0x1E, 0x0A, 0xFE, 0xF9, 0xF9,
		0x10, 0x11, 0x21, 0x0C, 0x0C, 0xFA, 0xFD, 0x00, 0x00, 0xFC, 0x0F, 0xE7, 0xDF, 0xCA, 0xAA, 0x84, 0x5D,
		0x10, 0x11, 0x21, 0x0C, 0x18, 0x3A, 0x1E, 0x0A, 0xFE, 0xF9, 0xF9, 0xFA, 0xFD, 0x00, 0x00, 0xFC, 0x0F,
		0x05, 0x11, 0x22, 0x01, 0x03, 0x3D,
		0x0A, 0x11, 0x23, 0x06, 0x00, 0x01, 0x05, 0x0B, 0x05, 0x02, 0x00,
		0x08, 0x11, 0x40, 0x04, 0x00, 0x38, 0x0D, 0xEB, 0x85 };

	static constexpr size_t SetupConfigSize = sizeof(SetupConfig);
};

template<const uint8_t pinCS,
	const uint8_t pinSDN,
	const uint8_t pinInterrupt,
	const uint8_t pinCLK = UINT8_MAX,
	const uint8_t pinMISO = UINT8_MAX,
	const uint8_t pinMOSI = UINT8_MAX,
	const uint8_t spiChannel = 0>
class Si4463Transceiver_433_70_250 final
	: public Si446xAbstractTransceiver<Si4463_433_70_250::RadioConfig, pinCS, pinSDN, pinInterrupt, pinCLK, pinMISO, pinMOSI, spiChannel>
{
private:
	using BaseClass = Si446xAbstractTransceiver<Si4463_433_70_250::RadioConfig, pinCS, pinSDN, pinInterrupt, pinCLK, pinMISO, pinMOSI, spiChannel>;

public:
	Si4463Transceiver_433_70_250(Scheduler& scheduler) : BaseClass(scheduler)
	{}

protected:
	virtual const uint8_t* GetConfigurationArray(size_t& configurationSize) final
	{
		configurationSize = Si4463_433_70_250::SetupConfigSize;

		return Si4463_433_70_250::SetupConfig;
	}
};
#endif