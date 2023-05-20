// LoLaRandom.h

#ifndef _LOLA_RANDOM_h
#define _LOLA_RANDOM_h

#include "LoLaCryptoDefinition.h"
#include "PCG.h"


/// <summary>
/// LoLa Random number generator.
/// Re-uses crypto hasher from LoLaEncoder, hence the template and constructor parameter.
/// </summary>
class LoLaRandom
{
private:
	PCG::pcg32_random_t Rng;


	IEntropySource* EntropySource; // Entropy source for CSPRNG.

public:
	LoLaRandom(IEntropySource* entropySource)
		: EntropySource(entropySource)
		, Rng()
	{
	}

	const bool Setup()
	{
		if (EntropySource != nullptr)
		{
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
		return GetRandomShort() % (maxValue + 1);
	}

	const uint32_t GetRandomLong()
	{
		return GetNextRandom();
	}

	const uint32_t GetRandomLong(const uint32_t maxValue)
	{
		return GetNextRandom() % (maxValue + 1);
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
			target[i] = GetRandomShort();
		}
	}

	void RandomReseed()
	{
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
		PCG::pcg32_random_r(&Rng);

#if defined(DEBUG_LOLA)
		Serial.println(F("Random Seeding complete."));
		if (idSize > 0)
		{
			Serial.print(F("\tUnique Id ("));
			Serial.print(idSize);
			Serial.println(F("bytes):"));

			Serial.println('\t');
			Serial.print('|');
			Serial.print('|');
			for (size_t i = 0; i < idSize; i++)
			{
				Serial.print(F("0x"));
				if (uniqueId[i] < 0x10)
				{
					Serial.print(0);
				}
				Serial.print(uniqueId[i], HEX);
				Serial.print('|');
			}
			Serial.println('|');
		}

		//Serial.print(F("\tSeed noise: "));
		//Serial.println(EntropySource->GetNoise());
		//Serial.print(F("\tFirst Random: "));
		//Serial.println(GetNextRandom());

		//uint32_t sum = 0;
		//const uint32_t start = micros();
		//static const uint32_t samples = 1000;
		//for (uint32_t i = 0; i < samples; i++)
		//{
		//	sum += GetRandomShort();
		//}
		//const uint32_t duration = micros() - start;

		//Serial.println(F("\tBenchmark "));
		//Serial.print(samples);
		//Serial.println(F(" samples took  "));
		//Serial.print(duration);
		//Serial.print(F(" us ("));
		//Serial.print(((uint64_t)duration * 1000) / samples);
		//Serial.println(F(" ns/byte )"));
#endif
	}
};
#endif