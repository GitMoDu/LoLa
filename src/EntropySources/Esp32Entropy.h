// Esp32Entropy.h

#ifndef _ESP32_ENTROPY_h
#define _ESP32_ENTROPY_h

#include <IEntropy.h>
#include <esp_mac.h>
#include <esp_random.h>

class Esp32Entropy final : public virtual IEntropy
{
private:
	static constexpr uint8_t MAC_LENGTH = 6;

	uint8_t PseudoMac[MAC_LENGTH]{};

public:
	Esp32Entropy() : IEntropy()
	{
		FillPseudoMac();
	}

	const uint8_t* GetUniqueId(uint8_t& idSize) final
	{
		idSize = MAC_LENGTH;

		return PseudoMac;
	}

	const uint32_t GetNoise() final
	{
		return esp_random();
	}

private:
	void FillPseudoMac()
	{
		esp_efuse_mac_get_default(PseudoMac);
	}
};
#endif