// LoLaCryptoEncoder.h

#ifndef _LOLACRYPTOENCODER_h
#define _LOLACRYPTOENCODER_h

#include <Crypto.h>
#include <BLAKE2s.h>
#include <Ascon128.h>
#include "utility/ProgMemUtil.h"

#include <LoLaDefinitions.h>
#include <PacketDriver\ILoLaSelector.h>
#include <LoLaCrypto\CryptoTokenGenerator.h>


// Responsible keeping the state of crypto encoding/decoding,
// MAC hash and key expansion.
class LoLaCryptoEncoder
{
private:
	static const uint8_t KeySize = 16; // Cypher.keySize()
	static const uint8_t IVSize = 16; // Cypher.ivSize()

	static const uint8_t MACSize = LOLA_LINK_CRYPTO_MAC_CRC_SIZE;

	// Protocol defined values
	static const uint8_t CryptoSeedSize = LOLA_LINK_CRYPTO_SEED_SIZE;
	static const uint8_t ChannelSeedSize = LOLA_LINK_CHANNEL_SEED_SIZE;
	static const uint8_t CryptoTokenSize = LOLA_LINK_CRYPTO_TOKEN_SIZE;

private:
	// Large enough to generate unique keys for each purpose.
	struct ExpandedKeyStruct
	{
		uint8_t Key[KeySize];
		uint8_t IV[IVSize];
		uint8_t CryptoSeed[CryptoSeedSize];
		uint8_t ChannelSeed[ChannelSeedSize];
	};

	union KeyArray
	{
		ExpandedKeyStruct Keys;
		uint8_t Array[sizeof(ExpandedKeyStruct)];
	} ExpandedKeySource;

	// If these are changed, update ProtocolVersionCalculator.
	Ascon128 Cypher;
	BLAKE2s Hasher;

	union ArrayToUint32
	{
		byte array[sizeof(uint32_t)];
		uint32_t uint;
	} HasherHelper;


	TOTPMillisecondsTokenGenerator<LOLA_LINK_SERVICE_LINKED_TOKEN_HOP_PERIOD_MILLIS, LOLA_LINK_CRYPTO_SEED_SIZE> CryptoTokenGenerator;
	TOTPMillisecondsTokenGenerator<LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS, LOLA_LINK_CHANNEL_SEED_SIZE> ChannelTokenGenerator;

	TimedHopProvider TimedHopEnabled;

public:
	LoLaCryptoEncoder() :
		Cypher(),
		Hasher(),
		CryptoTokenGenerator(),
		ChannelTokenGenerator()
	{
	}

	uint32_t HashArray(uint8_t* array, const uint8_t arrayLength)
	{
		Hasher.reset();
		Hasher.update(array, arrayLength);
		Hasher.finalize(HasherHelper.array, CryptoTokenSize);

		return HasherHelper.uint;
	}

	ITokenSource* GetChannelTokenSource()
	{
		return &ChannelTokenGenerator;
	}

	bool Setup(ISyncedClock* syncedClock)
	{
		return CryptoTokenGenerator.Setup(syncedClock, &TimedHopEnabled) &&
			ChannelTokenGenerator.Setup(syncedClock, &TimedHopEnabled);
	}

	void EnableTOTP()
	{
		TimedHopEnabled.TimedHopEnabled = true;
	}

	// Returns MAC/CRC.
	uint32_t Encode(uint8_t* message, const uint8_t messageLength, uint8_t* outputMessage)
	{
		ResetCypherBlock();
		Cypher.encrypt(outputMessage, message, messageLength);

		Hasher.reset();
		Hasher.update(message, messageLength);

		HasherHelper.uint = CryptoTokenGenerator.GetToken();
		Hasher.update(HasherHelper.array, CryptoTokenSize);

		Hasher.finalize(HasherHelper.array, CryptoTokenSize);

		return HasherHelper.uint;
	}

	bool Decode(uint8_t* message, const uint8_t messageLength, const uint32_t macCRC)
	{
		HasherHelper.uint = CryptoTokenGenerator.GetToken();

		Hasher.reset();
		Hasher.update(message, messageLength);

		Hasher.update(HasherHelper.array, CryptoTokenSize);

		Hasher.finalize(HasherHelper.array, CryptoTokenSize);

		if (macCRC == HasherHelper.uint)
		{
			ResetCypherBlock();
			Cypher.decrypt(message, message, messageLength);

			return true;
		}

		return true;
	}

	void Clear()
	{
		for (uint8_t i = 0; i < sizeof(ExpandedKeySource); i++)
		{
			((uint8_t*)&ExpandedKeySource)[i] = 0;
		}

		UpdateSeeds();

		TimedHopEnabled.TimedHopEnabled = false;
	}

	// Expands secret key + salt to fill the Expanded key (and all sub keys).
	// Can be optimized with a state preserving hasher expansion.
	void SetKeyWithSalt(uint8_t* secretKey, const uint32_t salt)
	{
		uint8_t ByteCount = 0;

		uint8_t Rounds = 1;
		uint8_t Chunk = 0;

		while (ByteCount < sizeof(ExpandedKeySource))
		{
			Hasher.clear();

			// Secret key.
			Hasher.update(secretKey, KeySize);

			// Salt.
			// We resalt Round times because we're stretching the original hash state, without directly restoring the state.
			// Fortunately, our hash is fast.
			for (uint8_t i = 0; i < Rounds; i++)
			{
				HasherHelper.uint = salt;
				Hasher.update(HasherHelper.array, sizeof(uint32_t));
			}

			Chunk = min(sizeof(ExpandedKeySource) - ByteCount, Hasher.hashSize());

			Hasher.finalize(&ExpandedKeySource.Array[ByteCount], Chunk);

			if (ByteCount >= sizeof(ExpandedKeySource))
			{
				// We're done.
				break;
			}
			else
			{
				ByteCount += Chunk;
				Rounds++;
			}
		}
		Hasher.clear();

#ifdef DEBUG_LOLA
		Cypher.clear();

		// Test if key is accepted.
		if (!Cypher.setKey(ExpandedKeySource.Keys.Key, KeySize))
		{
			Serial.println(F("Failed to set Key."));
		}

		// Test setting IV.
		if (!Cypher.setIV(ExpandedKeySource.Keys.IV, KeySize))
		{
			Serial.println(F("Failed to set IV."));
		}
		Cypher.clear();
#endif

		UpdateSeeds();
	}

private:
	void ResetCypherBlock()
	{
		Cypher.setKey(ExpandedKeySource.Keys.Key, KeySize);
		Cypher.setIV(ExpandedKeySource.Keys.IV, IVSize);
		//TODO: Consider adding auth data if performance is ok.
	}

	void UpdateSeeds()
	{
		// Update token seeds for token generators.
		Hasher.reset();
		Hasher.update(ExpandedKeySource.Keys.CryptoSeed, CryptoSeedSize);
		Hasher.finalize(HasherHelper.array, CryptoTokenSize);

		CryptoTokenGenerator.SetSeed(HasherHelper.uint);

		Hasher.reset();
		Hasher.update(ExpandedKeySource.Keys.ChannelSeed, ChannelSeedSize);
		Hasher.finalize(HasherHelper.array, CryptoTokenSize);
		ChannelTokenGenerator.SetSeed(HasherHelper.uint);
	}

#ifdef DEBUG_LOLA
public:
	void Debug(Stream* serial)
	{
		serial->print("Ascon128");
	}

	void PrintKey(Stream* serial)
	{
		/*serial->print("Secret Key:\n\t|");
		for (uint8_t i = 0; i < KeySize; i++)
		{
			serial->print(ExpandedKeySource.Keys.Key[i]);
			serial->print('|');
		}
		serial->println();*/

		serial->print("Full Key:\n\t|");
		for (uint8_t i = 0; i < sizeof(ExpandedKeyStruct); i++)
		{
			serial->print(ExpandedKeySource.Array[i]);
			serial->print('|');
		}
		serial->println();
	}
#endif
};
#endif
