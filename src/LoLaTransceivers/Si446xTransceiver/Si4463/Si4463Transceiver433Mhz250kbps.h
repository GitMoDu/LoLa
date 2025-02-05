#ifndef _SI4463_TRANSCEIVER_433_250_h
#define _SI4463_TRANSCEIVER_433_250_h

#include "../Si446xAbstractTransceiver.h"

namespace Si4463_433_250
{
	struct RadioConfig
	{
		static constexpr uint16_t TRANSCEIVER_ID = 0x4463;

		static constexpr uint32_t BaseFrequency = 433075000;
		static constexpr uint32_t BitRate = 250000;

		static constexpr uint8_t ChannelCount = 68;

		static constexpr uint8_t TxPowerMin = 7;
		static constexpr uint8_t TxPowerMax = TxPowerMin;

		static constexpr uint16_t TTRX_SHORT = 1206;
		static constexpr uint16_t TTRX_LONG = 2050;

		static constexpr uint8_t RSSI_MIN = 60;
		static constexpr uint8_t RSSI_MAX = 160;
	};

	static constexpr uint8_t SetupConfig[]
	{
		0x07, 0x02, 0x01, 0x00, 0x01, 0xC9, 0xC3, 0x80,
		0x08, 0x13, 0x41, 0x41, 0x21, 0x20, 0x67, 0x4B, 0x00,
		0x05, 0x11, 0x00, 0x01, 0x00, 0x52,
		0x05, 0x11, 0x00, 0x01, 0x03, 0x60,
		0x10, 0x11, 0x20, 0x0C, 0x00, 0x03, 0x00, 0x07, 0x02, 0x71, 0x00, 0x05, 0xC9, 0xC3, 0x80, 0x00, 0x00,
		0x05, 0x11, 0x20, 0x01, 0x0C, 0x46,
		0x0C, 0x11, 0x20, 0x08, 0x18, 0x01, 0x00, 0x08, 0x03, 0x80, 0x00, 0xB0, 0x10,
		0x0D, 0x11, 0x20, 0x09, 0x22, 0x00, 0x4E, 0x06, 0x8D, 0xB9, 0x00, 0x00, 0x02, 0xC0,
		0x0B, 0x11, 0x20, 0x07, 0x2C, 0x00, 0x12, 0x00, 0x23, 0x01, 0x5C, 0xA0,
		0x05, 0x11, 0x20, 0x01, 0x35, 0xE2,
		0x0D, 0x11, 0x20, 0x09, 0x38, 0x11, 0x11, 0x11, 0x00, 0x1A, 0x20, 0x00, 0x00, 0x28,
		0x0C, 0x11, 0x20, 0x08, 0x42, 0xA4, 0x03, 0xD6, 0x03, 0x00, 0x7B, 0x01, 0x80,
		0x05, 0x11, 0x20, 0x01, 0x4E, 0x22,
		0x05, 0x11, 0x20, 0x01, 0x51, 0x0A,
		0x10, 0x11, 0x21, 0x0C, 0x00, 0x7E, 0x64, 0x1B, 0xBA, 0x58, 0x0B, 0xDD, 0xCE, 0xD6, 0xE6, 0xF6, 0x00,
		0x10, 0x11, 0x21, 0x0C, 0x0C, 0x03, 0x03, 0x15, 0xF0, 0x3F, 0x00, 0x7E, 0x64, 0x1B, 0xBA, 0x58, 0x0B,
		0x10, 0x11, 0x21, 0x0C, 0x18, 0xDD, 0xCE, 0xD6, 0xE6, 0xF6, 0x00, 0x03, 0x03, 0x15, 0xF0, 0x3F, 0x00,
		0x05, 0x11, 0x22, 0x01, 0x03, 0x3D,
		0x0B, 0x11, 0x23, 0x07, 0x00, 0x2C, 0x0E, 0x0B, 0x04, 0x0C, 0x73, 0x03,
		0x0C, 0x11, 0x40, 0x08, 0x00, 0x37, 0x09, 0x00, 0x00, 0x06, 0xD4, 0x20, 0xFE,
		0x08, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x05, 0x17, 0x56, 0x10, 0xCA, 0xF0,
		0x05, 0x17, 0x13, 0x10, 0xCA, 0xF0,
		0x05, 0x11, 0x00, 0x01, 0x01, 0x00,
		0x05, 0x11, 0x00, 0x01, 0x03, 0x60,
		0x07, 0x11, 0x01, 0x03, 0x00, 0x03, 0x30, 0x00,
		0x08, 0x11, 0x02, 0x04, 0x00, 0x0A, 0x09, 0x04, 0x06,
		0x0D, 0x11, 0x10, 0x09, 0x00, 0x0C, 0x14, 0x00, 0x0F, 0x31, 0x00, 0x00, 0x00, 0x00,
		0x09, 0x11, 0x11, 0x05, 0x00, 0x01, 0xB4, 0x4B, 0x00, 0x00,
		0x0B, 0x11, 0x12, 0x07, 0x00, 0x80, 0x01, 0x08, 0xFF, 0xFF, 0x00, 0x00,
		0x10, 0x11, 0x12, 0x0C, 0x08, 0x2A, 0x01, 0x00, 0x30, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00,
		0x10, 0x11, 0x12, 0x0C, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x11, 0x12, 0x0C, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x0D, 0x11, 0x12, 0x09, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x11, 0x20, 0x0C, 0x00, 0x03, 0x00, 0x07, 0x26, 0x25, 0xA0, 0x01, 0xC9, 0xC3, 0x80, 0x00, 0x06,
		0x05, 0x11, 0x20, 0x01, 0x0C, 0xD4,
		0x0C, 0x11, 0x20, 0x08, 0x18, 0x01, 0x00, 0x08, 0x03, 0x80, 0x00, 0x00, 0x30,
		0x0D, 0x11, 0x20, 0x09, 0x22, 0x00, 0x78, 0x04, 0x44, 0x44, 0x07, 0xFF, 0x02, 0x00,
		0x0B, 0x11, 0x20, 0x07, 0x2C, 0x00, 0x23, 0x8F, 0xFF, 0x00, 0x84, 0xA0,
		0x05, 0x11, 0x20, 0x01, 0x35, 0xE2,
		0x0D, 0x11, 0x20, 0x09, 0x38, 0x22, 0x0D, 0x0D, 0x00, 0x1A, 0x0C, 0xCD, 0x00, 0x28,
		0x0D, 0x11, 0x20, 0x09, 0x42, 0xA4, 0x03, 0xD6, 0x03, 0x00, 0x20, 0x01, 0x80, 0x30,
		0x05, 0x11, 0x20, 0x01, 0x4C, 0x02,
		0x05, 0x11, 0x20, 0x01, 0x4E, 0x40,
		0x05, 0x11, 0x20, 0x01, 0x51, 0x0A,
		0x10, 0x11, 0x21, 0x0C, 0x00, 0xE7, 0xDF, 0xCA, 0xAA, 0x84, 0x5D, 0x3A, 0x1E, 0x0A, 0xFE, 0xF9, 0xF9,
		0x10, 0x11, 0x21, 0x0C, 0x0C, 0xFA, 0xFD, 0x00, 0x00, 0xFC, 0x0F, 0xE7, 0xDF, 0xCA, 0xAA, 0x84, 0x5D,
		0x10, 0x11, 0x21, 0x0C, 0x18, 0x3A, 0x1E, 0x0A, 0xFE, 0xF9, 0xF9, 0xFA, 0xFD, 0x00, 0x00, 0xFC, 0x0F,
		0x08, 0x11, 0x22, 0x04, 0x00, 0x08, 0x7F, 0x00, 0x5D,
		0x0B, 0x11, 0x23, 0x07, 0x00, 0x01, 0x05, 0x0B, 0x05, 0x02, 0x00, 0x03,
		0x10, 0x11, 0x30, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x0C, 0x11, 0x40, 0x08, 0x00, 0x38, 0x0D, 0xEB, 0x85, 0x06, 0xD4, 0x20, 0xFE,
		0x00
	};
};

/// <summary>
/// SI4463 433 MHz LoLa Transceiver @ 250 kbps.
/// </summary>
/// <typeparam name="pinCS"></typeparam>
/// <typeparam name="pinSDN"></typeparam>
/// <typeparam name="pinInterrupt"></typeparam>
template<const uint8_t pinCS,
	const uint8_t pinSDN,
	const uint8_t pinInterrupt>
class Si4463Transceiver433Mhz250kbps final
	: public Si446xAbstractTransceiver<Si4463_433_250::RadioConfig, pinCS, pinSDN, pinInterrupt>
{
private:
	using Base = Si446xAbstractTransceiver<Si4463_433_250::RadioConfig, pinCS, pinSDN, pinInterrupt>;

public:
	Si4463Transceiver433Mhz250kbps(TS::Scheduler& scheduler, SPIClass& spiInstance)
		: Base(scheduler, spiInstance)
	{
	}

protected:
	virtual const uint8_t* GetConfigurationArray(size_t& size) final
	{
		size = sizeof(Si4463_433_250::SetupConfig);

		return Si4463_433_250::SetupConfig;
	}
};
#endif