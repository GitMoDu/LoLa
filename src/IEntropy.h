// IEntropy.h
#ifndef _I_ENTROPY_h
#define _I_ENTROPY_h

#include <stdint.h>

/// <summary>
/// Interface for entropy sourcing.
/// </summary>
class IEntropy
{
public:
	virtual const uint8_t* GetUniqueId(uint8_t& idSize)
	{
		idSize = 0;
		return nullptr;
	}

	virtual const uint32_t GetNoise() { return 0; }
};

/// <summary>
/// Abstract 1 bit entropy extractor.
/// Provides separate setup and teardown for entropy source.
/// </summary>
class Abstract1BitEntropy : public virtual IEntropy
{
private:
	static constexpr uint8_t BITS_IN_NOISE = sizeof(uint32_t) * 8;

protected:
	virtual void OpenEntropy() { }
	virtual void CloseEntropy() { }

	virtual bool Get1BitEntropy() { return false; }

public:
	Abstract1BitEntropy() : IEntropy()
	{}

	/// <summary>
	/// The ADCs least significant bit is the source of entropy.
	/// Using a XOR'd pair of 1 bit measures moves existing bias closer to ~50%.
	/// </summary>
	/// <returns>32 bits of entropy.</returns>
	virtual const uint32_t GetNoise() final
	{
		OpenEntropy();

		uint32_t noise = 0;
		for (uint_fast8_t i = 0; i < BITS_IN_NOISE; i++)
		{
			noise |= ((uint32_t)((Get1BitEntropy() ^ Get1BitEntropy()) & 0x01)) << i;
		}

		CloseEntropy();

#if defined(DEBUG_ENTROPY_SOURCE)
		Serial.print(F("Noise\t|"));
		for (size_t i = 0; i < sizeof(uint32_t); i++)
		{
			if (((uint8_t*)&noise)[i] < 0x10)
			{
				Serial.print(0);
			}
			Serial.print(((uint8_t*)&noise)[i], HEX);
			Serial.print('|');
		}
		Serial.print(F("(0b"));
		Serial.print(noise, BIN);
		Serial.println(')');
#endif

		return noise;
	}
};
#endif