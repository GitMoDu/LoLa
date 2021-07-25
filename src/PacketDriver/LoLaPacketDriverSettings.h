// LoLaPacketDriverSettings.h

#ifndef _LOLA_PACKET_DRIVER_SETTINGS_h
#define _LOLA_PACKET_DRIVER_SETTINGS_h

#include <stdint.h>

struct LoLaPacketDriverSettings
{
public:
	const uint32_t ETTM;
	const int16_t RSSIMax;
	const int16_t RSSIMin;

	const uint8_t ChannelMax;
	const uint8_t ChannelMin;

	const uint8_t TransmitPowerMax;
	const uint8_t TransmitPowerMin;

	//const uint8_t MaxPacketSize;
public:
	LoLaPacketDriverSettings(const uint32_t ettm, 
		const int16_t rssiMax, const int16_t rssiMin, 
		const uint8_t channelMax, const uint8_t channelMin,
		const uint8_t transmitPowerMax, const uint8_t transmitPowerMin
	)
		:ETTM(ettm)
		, RSSIMax(rssiMax)
		, RSSIMin(rssiMin)
		, ChannelMax(channelMax)
		, ChannelMin(channelMin)
		, TransmitPowerMax(transmitPowerMax)
		, TransmitPowerMin(transmitPowerMin)
		//, MaxPacketSize()
	{
	}
};
#endif