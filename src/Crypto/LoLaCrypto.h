// LoLaCrypto.h

#ifndef _LOLACRYPTO_h
#define _LOLACRYPTO_h

#include <Arduino.h>
#include <Crypto\TinyCRC.h>

class LoLaCrypto
{
private:
	///Event callbacks
	uint32_t(*GetCryptoSeedCall)(void) = nullptr;
	///

protected:
	///CRC validation.
	TinyCrcModbus8 CalculatorCRC;
	uint8_t CRCIndex = 0;
	///

	uint8_t CryptoIndex = 0;

	uint8_t GetEncryptedValue(const uint8_t value, const uint8_t index)
	{
		return value;
	}
	uint8_t GetDecryptedValue(const uint8_t value, const uint8_t index)
	{
		return value;
	}

	bool IsCryptoEnabled()
	{
		return false;
	}

public:
	void SetCryptoSeedCall(uint32_t(&callGetCryptoSeed)(void))
	{
		GetCryptoSeedCall = callGetCryptoSeed;
	}

	uint32_t GetCryptoSeed()
	{
		if (GetCryptoSeedCall != nullptr)
		{
			return GetCryptoSeedCall();
		}
		else
		{
			return 0;
		}
	}

	uint32_t GetRandomDuration(const uint32_t min, const uint32_t max)
	{
		randomSeed(millis());
		return random(min, max);
	}
};


#endif

