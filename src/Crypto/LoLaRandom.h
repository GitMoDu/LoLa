// LoLaRandom.h

#ifndef _LOLA_RANDOM_h
#define _LOLA_RANDOM_h

#include "../EntropySources/IEntropy.h"

/*
* https://www.pcg-random.org
*/
#include "PCG\PCG.h"


/// <summary>
/// LoLa Random number generator.
/// Cryptographic Secure(ish) Random Number Generator.
/// Based on https://www.pcg-random.org
/// </summary>
class LoLaRandom
{
private:
	/// <summary>
	/// Random stream generator.
	/// </summary>
	PCG::pcg32_random_t Rng{};

	/// <summary>
	/// Entropy source for CSPRNG.
	/// </summary>
	IEntropy* EntropySource;

public:
	LoLaRandom(IEntropy* entropy)
		: EntropySource(entropy)
	{}

	const bool Setup()
	{
		if (EntropySource != nullptr)
		{
			if (EntropySource->GetNoise() == 0
				&& EntropySource->GetNoise() == 0)
			{
#if defined(DEBUG_LOLA)
				Serial.println(F("Entropy source not providing entropy."));
#endif
				return false;
			}

			RandomSeed();

			return true;
		}

		return false;
	}

	const uint8_t GetRandomShort()
	{
		return (uint8_t)GetNextRandom();
	}

	const uint8_t GetRandomShort(const uint8_t maxValue)
	{
		return (uint8_t)GetRandomLong(maxValue);
	}

	const uint32_t GetRandomLong()
	{
		return GetNextRandom();
	}

	/// <summary>
	/// Ranging done using Biased Integer Multiplication.
	/// https://www.pcg-random.org/posts/bounded-rands.html
	/// </summary>
	/// <param name="maxValue"></param>
	/// <returns>Random value [0 ; maxValue].</returns>
	const uint32_t GetRandomLong(const uint32_t maxValue)
	{
		if (maxValue == UINT32_MAX)
		{
			return GetNextRandom();
		}
		else
		{
			return (uint32_t)(((uint64_t)GetNextRandom() * maxValue) >> 32);
		}
	}

	/// <summary>
	/// 
	/// </summary>
	/// <typeparam name="target"></typeparam>
	/// <typeparam name="size"></typeparam>
	void GetRandomStreamCrypto(uint8_t* target, const uint8_t size)
	{
		for (uint_fast8_t i = 0; i < size; i++)
		{
			target[i] = (uint8_t)GetNextRandom();
		}
	}

	void RandomReseed()
	{
		Rng.inc += EntropySource->GetNoise();
		GetNextRandom();
	}

private:
	const uint32_t GetNextRandom()
	{
		return PCG::pcg32_random_r(&Rng);
	}

	void RandomSeed()
	{
		uint8_t idSize = 0;
		const uint8_t* uniqueId = EntropySource->GetUniqueId(idSize);

		Rng.state = 0;
		Rng.inc = 0;
		if (uniqueId != nullptr && idSize > 0)
		{
			for (uint_least8_t i = 0; i < idSize; i++)
			{
				Rng.state += ((uint64_t)uniqueId[i] << (8 * (i % sizeof(uint64_t))));
			}
		}
		Rng.state += EntropySource->GetNoise();
		Rng.state += (uint64_t)EntropySource->GetNoise() << 32;
		Rng.inc += EntropySource->GetNoise();

		PCG::pcg32_random_r(&Rng);
	}
};
#endif