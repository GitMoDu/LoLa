// Esp8266EntropySource.H

#ifndef _ESP8266_ENTROPY_SOURCE_h
#define _ESP8266_ENTROPY_SOURCE_h

#include <IEntropySource.h>
#include <wl_definitions.h>
#include <esp8266_peri.h>

class Esp8266EntropySource : public virtual IEntropySource
{
private:
	static constexpr uint8_t MAC_LENGTH = WL_MAC_ADDR_LENGTH;

	uint8_t PseudoMac[MAC_LENGTH];

public:
	Esp8266EntropySource() : IEntropySource(), PseudoMac()
	{
		FillPseudoMac();
	}

	virtual const uint8_t* GetUniqueId(uint8_t& idSize) final
	{
		idSize = MAC_LENGTH;

		return PseudoMac;
	}

	virtual const uint32_t GetNoise() final
	{
		return RANDOM_REG32;
	}

private:
	void FillPseudoMac()
	{
		PseudoMac[0] = MAC0 & UINT32_MAX;
		PseudoMac[1] = (MAC0 >> 8) & UINT32_MAX;
		PseudoMac[2] = (MAC0 >> 16) & UINT32_MAX;
		PseudoMac[3] = (MAC0 >> 24) & UINT32_MAX;

		PseudoMac[4] = MAC1 & UINT32_MAX;
		PseudoMac[5] = (MAC1 >> 8) & UINT32_MAX;
	}
};
#endif