// LoLaRandom.h

#ifndef _LOLA_RANDOM_h
#define _LOLA_RANDOM_h

#include "LoLaCryptoDefinition.h"


/// <summary>
/// LoLa Random number generator.
/// Re-uses crypto hasher from LoLaEncoder, hence the template and constructor parameter.
/// </summary>
/// <typeparam name="HasherType">Hasher must output at least 32 bytes.</typeparam>
template<typename HasherType>
class LoLaRandom
{
private:
	union ArrayToUint32
	{
		uint8_t Array[sizeof(uint32_t)];
		uint32_t UInt = 0;
	};

	static constexpr uint8_t ENTROPY_POOL_SIZE = 8;
	static constexpr uint8_t INTEGER_SIZE = 4;

	HasherType& Hasher;

	IEntropySource* EntropySource; // Entropy source for CSPRNG.

private:
	uint8_t EntropyPool[ENTROPY_POOL_SIZE]{};

	ArrayToUint32 EntropySalt{};
	ArrayToUint32 RandomHelper{};

public:
	LoLaRandom(HasherType& hasher, IEntropySource* entropySource)
		: Hasher(hasher)
		, EntropySource(entropySource)
	{}

	const bool Setup()
	{
		if (EntropySource != nullptr)
		{
			RandomSeed();

			return true;
		}

		return false;
	}

	/// <summary>
	/// Inspired by random.c (Linux).
	/// </summary>
	/// <returns></returns>
	const uint32_t GetNextRandom()
	{
		EntropySalt.UInt += 1;
		Hasher.reset(INTEGER_SIZE);
		Hasher.update(EntropySalt.Array, INTEGER_SIZE);
		Hasher.update(EntropyPool, ENTROPY_POOL_SIZE);
		Hasher.finalize(RandomHelper.Array, INTEGER_SIZE);

		EntropySalt.UInt += 1;
		Hasher.reset(ENTROPY_POOL_SIZE);
		Hasher.update(EntropySalt.Array, INTEGER_SIZE);
		Hasher.update(EntropyPool, ENTROPY_POOL_SIZE);
		Hasher.finalize(EntropyPool, ENTROPY_POOL_SIZE);

		return RandomHelper.UInt;
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
	/// <typeparam name="size">Max size 32.</typeparam>
	void GetRandomStreamCrypto(uint8_t* target, const uint8_t size)
	{
		ArrayToUint32 helper;
		helper.UInt = GetRandomLong();

		Hasher.reset(size);
		Hasher.update(helper.Array, sizeof(uint32_t));
		Hasher.finalize(target, size);
	}

	void RandomReseed()
	{
		GetNextRandom();
	}

private:
	void RandomSeed()
	{
		uint8_t idSize = 0;
		const uint8_t* uniqueId = EntropySource->GetUniqueId(idSize);

		EntropySalt.UInt = EntropySource->GetNoise();

		Hasher.reset(ENTROPY_POOL_SIZE);
		Hasher.update(EntropySalt.Array, sizeof(uint32_t));
		if (uniqueId != nullptr && idSize > 0)
		{
			Hasher.update(uniqueId, idSize);
		}
		Hasher.finalize(EntropyPool, ENTROPY_POOL_SIZE);

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

		Serial.print(F("\tSeed noise:"));
		Serial.println(EntropySalt.UInt);
#endif

		EntropySalt.UInt++;
	}
};
#endif