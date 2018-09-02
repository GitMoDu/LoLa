// PseudoMacGenerator.h

#ifndef _PSEUDOMACGENERATOR_h
#define _PSEUDOMACGENERATOR_h

#include <stdint.h>
#include <Crypto\TinyCRC.h>

#define LOLA_INVALID_PMAC						UINT32_MAX

//#if defined(ARDUINO_ARCH_STM32F1)
//#elif defined(ARDUINO_ARCH_AVR)

class PseudoMacGenerator
{
private:
	uint32_t CachedPMAC = LOLA_INVALID_PMAC;
	TinyCrc32 Hasher;

public:
	PseudoMacGenerator()
	{
	}

	bool SetId(uint8_t *idPointer, const uint8_t idLength)
	{
		if (idPointer != nullptr && idLength > 0)
		{
			Hasher.Reset();
			Hasher.Update(idPointer, idLength);
			CachedPMAC = Hasher.GetCurrent();

			return true;
		}
	}

	uint32_t GetPMAC()
	{
		return CachedPMAC;
	}

};
#endif