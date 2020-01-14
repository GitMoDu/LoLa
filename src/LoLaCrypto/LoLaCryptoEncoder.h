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
	// From Ascon128 spec
	static const uint8_t KeySize = 16;
	static const uint8_t IVSize = 16;

	static const uint8_t MACSize = LOLA_LINK_CRYPTO_MAC_CRC_SIZE;

	// Protocol defined values
	static const uint8_t CryptoSeedSize = LOLA_LINK_CRYPTO_SEED_SIZE;
	static const uint8_t CryptoTokenSize = LOLA_LINK_CRYPTO_TOKEN_SIZE;
	static const uint8_t ChannelSeedSize = LOLA_LINK_CHANNEL_SEED_SIZE;
	static const uint8_t CryptoTokenPeriodMillis = LOLA_LINK_SERVICE_LINKED_TOKEN_HOP_PERIOD_MILLIS;

private:
	// Large enough to generate unique keys for each purpose.
	struct ExpandedKeyStruct
	{
		uint8_t Key[KeySize];
		uint8_t IV[IVSize];
		uint8_t CryptoSeed[CryptoSeedSize];
		uint8_t ChannelSeed[ChannelSeedSize];
	};

	ExpandedKeyStruct ExpandedKeySource;

	// If these are changed, update ProtocolVersionCalculator.
	Ascon128 Cypher;
	BLAKE2s Hasher;

	union ArrayToUint32 {
		byte array[sizeof(uint32_t)]; //TOKEN_SIZE
		uint32_t uint;
	} HasherHelper;

	static const uint8_t TokenSize = LOLA_LINK_CRYPTO_TOKEN_SIZE; // Token is 32 bit.
	static const uint8_t AuthDataSize = TokenSize;

	TOTPMillisecondsTokenGenerator CryptoTokenGenerator;
	TOTPMillisecondsTokenGenerator ChannelTokenGenerator;

	bool TOTPEnabled = false;

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
		return CryptoTokenGenerator.Setup(syncedClock, &TOTPEnabled) &&
			ChannelTokenGenerator.Setup(syncedClock, &TOTPEnabled) &&
			sizeof(ExpandedKeySource) <= Hasher.hashSize();
	}

	void EnableTOTP()
	{
		TOTPEnabled = true;
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

		TOTPEnabled = false;
	}

	void SetKeyWithSalt(uint8_t* secretKey, const uint32_t salt)
	{
		// Adding shared entropy and expanding key to fill in all slots.
		Hasher.clear();

		// Secret key.
		Hasher.update(secretKey, KeySize);

		// Salt.
		HasherHelper.uint = salt;
		Hasher.update(HasherHelper.array, sizeof(uint32_t));

		// sizeof(ExpandedKeySource) <= Hasher.hashSize()
		// We don't even need the full hash, let alone another iteration.
		Hasher.finalize((uint8_t*)&ExpandedKeySource, sizeof(ExpandedKeySource));

#ifdef DEBUG_LOLA
		Cypher.clear();

		// Test if key is accepted.
		if (!Cypher.setKey(ExpandedKeySource.Key, KeySize))
		{
			Serial.println(F("Failed to set Key."));
		}

		// Test setting IV.
		if (!Cypher.setIV(ExpandedKeySource.IV, KeySize))
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
		Cypher.setKey(ExpandedKeySource.Key, KeySize);
		Cypher.setIV(ExpandedKeySource.IV, IVSize);
		//TODO: Consider adding auth data if performance is ok.
	}

	void UpdateSeeds()
	{
		// Update token seeds for token generators.
		Hasher.reset();
		Hasher.update(ExpandedKeySource.CryptoSeed, CryptoSeedSize);
		Hasher.finalize(HasherHelper.array, CryptoTokenSize);

		CryptoTokenGenerator.SetSeed(HasherHelper.uint);

		Hasher.reset();
		Hasher.update(ExpandedKeySource.ChannelSeed, ChannelSeedSize);
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
		serial->print("Secret Key:\n\t|");
		for (uint8_t i = 0; i < KeySize; i++)
		{
			serial->print(ExpandedKeySource.Key[i]);
			serial->print('|');
		}
	}
#endif
};
#endif
