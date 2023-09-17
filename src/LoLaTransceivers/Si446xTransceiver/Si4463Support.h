// Si4463Support.h

#ifndef _LOLA_SI4463_SUPPORT_h
#define _LOLA_SI4463_SUPPORT_h

#include <Si446x.h>
#include "radio_config.h"

struct LoLaSi4463Config
{
	static constexpr uint16_t RADIO_CONFIG_868_CENTER_FREQUENCY = 868;
	static constexpr uint8_t RADIO_CONFIG_868_CHANNEL_COUNT = 99;

	static constexpr uint16_t RADIO_CONFIG_433_CENTER_FREQUENCY = 433;
	static constexpr uint8_t RADIO_CONFIG_433_CHANNEL_COUNT = 35;

	enum class ConfigEnum : uint8_t
	{
		LoLa433 = 0x46,
		LoLa866 = 0x23
	};

	static constexpr uint8_t GetRadioConfigChannelCount(const uint8_t x1,
		const uint8_t x2,
		const uint8_t x3,
		const uint8_t x4,
		const uint8_t x5)
	{
		return ((GetConfigId(x1, x2, x3, x4, x5) == (uint8_t)ConfigEnum::LoLa866) * RADIO_CONFIG_868_CHANNEL_COUNT)
			| ((GetConfigId(x1, x2, x3, x4, x5) == (uint8_t)ConfigEnum::LoLa433) * RADIO_CONFIG_433_CHANNEL_COUNT);
	}

	static constexpr uint8_t ValidateConfig(const uint8_t x1,
		const uint8_t x2,
		const uint8_t x3,
		const uint8_t x4,
		const uint8_t x5)
	{
		return (((GetConfigId(x1, x2, x3, x4, x5) == (uint8_t)ConfigEnum::LoLa866))
			| ((GetConfigId(x1, x2, x3, x4, x5) == (uint8_t)ConfigEnum::LoLa433))) > 0;
	}

	static constexpr uint8_t GetConfigId(const uint8_t x1,
		const uint8_t x2,
		const uint8_t x3,
		const uint8_t x4,
		const uint8_t x5)
	{
		return x5; // Clock Divisor?
	}
};
#endif