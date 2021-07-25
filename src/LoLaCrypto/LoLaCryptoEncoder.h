// LoLaCryptoEncoder.h

#ifndef _LOLACRYPTOENCODER_h
#define _LOLACRYPTOENCODER_h

#include <Crypto.h>
#include <BLAKE2s.h>
#include <Ascon128.h>
#include "utility/ProgMemUtil.h"

#include <LoLaDefinitions.h>
#include <LoLaCrypto\CryptoTokenGenerator.h>


// Responsible keeping the state of crypto encoding/decoding,
// MAC hash and key expansion.
// Encrypt-then-MAC.
class LoLaCryptoEncoder
{
private:
	static const uint8_t KeySize = 16; // Cypher.keySize()
	static const uint8_t IVSize = 16; // Cypher.ivSize()

	// 4 Byte Message Authentication Code.
	static const uint8_t MACSize = 4;

private:
	// If these are changed, update ProtocolVersionCalculator.
	Ascon128 Cypher;
	BLAKE2s Hasher; // 32 Byte hasher ouput.

	union ArrayToUint32
	{
		byte array[sizeof(uint32_t)];
		uint32_t uint;
	};

	// Large enough to generate unique keys for each purpose.
	struct ExpandedKeyStruct
	{
		uint8_t Key[KeySize];
		uint8_t IV[IVSize];
		uint8_t CryptoSeed[sizeof(uint32_t)];
		uint8_t ChannelSeed[sizeof(uint32_t)];
	};

	union KeyArray
	{
		ExpandedKeyStruct Keys;
		uint8_t Array[sizeof(ExpandedKeyStruct)];
	} ExpandedKeySource;

	// Runtime Flags.
	bool TokenHopEnabled = false;
	bool EncryptionEnabled = false;

public:
	TOTPSecondsTokenGenerator LinkTokenGenerator;
	TOTPMillisecondsTokenGenerator ChannelTokenGenerator;

public:
	LoLaCryptoEncoder(ISyncedClock* syncedClock)
		: Cypher()
		, Hasher()
		, LinkTokenGenerator(syncedClock)
		, ChannelTokenGenerator(syncedClock, LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS)
	{
	}

	void StartTokenHop()
	{
#ifdef LOLA_LINK_USE_TOKEN_HOP
		TokenHopEnabled = true;
#endif
	}

	void StartEncryption()
	{
#ifdef LOLA_LINK_USE_ENCRYPTION
		EncryptionEnabled = true;
#endif
	}

	bool Setup()
	{
		//TODO: Use this time to source some nice entropy to see the RNG.
		Clear();

		return true;
	}

	// Returns MAC/CRC.
	uint32_t Encode(uint8_t* message, const uint8_t messageLength, const uint8_t forwardsMillis)
	{
		ArrayToUint32 Helper;

		// Start MAC.
		Hasher.reset();

		if (EncryptionEnabled)
		{
			ResetCypherBlock();
			Cypher.encrypt(message, message, messageLength);

			// Message content, encrypted.
			Hasher.update(message, messageLength);

			// Get MAC key.
			if (TokenHopEnabled)
			{
				Helper.uint = LinkTokenGenerator.GetTokenFromAhead(forwardsMillis);
			}
			else
			{
				Helper.uint = LinkTokenGenerator.GetSeed();
			}

			// MAC key.
			Hasher.update(Helper.array, sizeof(uint32_t));
		}
		else
		{
			// Message content.
			Hasher.update(message, messageLength);
		}

		// MAC ready for output.
		Hasher.finalize(Helper.array, sizeof(uint32_t));

		return Helper.uint;
	}

	bool Decode(uint8_t* message, const uint8_t messageLength, const uint32_t macCRC, const uint8_t backwardsMillis)
	{
		ArrayToUint32 Helper;

		// Start MAC.
		Hasher.reset();

		// Message content.
		Hasher.update(message, messageLength);

		if (EncryptionEnabled)
		{
			// Get MAC key.
			if (TokenHopEnabled)
			{
				Helper.uint = LinkTokenGenerator.GetTokenFromEarlier(backwardsMillis);
			}
			else
			{
				Helper.uint = LinkTokenGenerator.GetSeed();
			}

			// Crypto Token.
			Hasher.update(Helper.array, sizeof(uint32_t));
		}

		// MAC ready for comparison.
		Hasher.finalize(Helper.array, sizeof(uint32_t));

		if (macCRC == Helper.uint)
		{
			if (EncryptionEnabled)
			{
				ResetCypherBlock();
				Cypher.decrypt(message, message, messageLength);
			}
			return true;
		}

		return false;
	}

	void Clear()
	{
		EncryptionEnabled = false;
		TokenHopEnabled = false;

		for (uint8_t i = 0; i < sizeof(ExpandedKeySource); i++)
		{
			((uint8_t*)&ExpandedKeySource)[i] = 0;
		}

		LinkTokenGenerator.SetSeed(0);
		ChannelTokenGenerator.SetSeed(0);
	}

	const uint32_t SignContentSharedKey(const uint32_t content)
	{
		ArrayToUint32 Helper;
		Helper.uint = content;

		// Start hasher with Shared Key.
		//Hasher.resetHMAC(ExpandedKeySource.Keys.Key, KeySize);
		Hasher.reset();

		// Message Content.
		Hasher.update(Helper.array, sizeof(uint32_t));

		// MAC ready for comparison.
		//Hasher.finalizeHMAC(Helper.array, sizeof(uint32_t), Helper.array, sizeof(uint32_t));
		Hasher.finalize(Helper.array, sizeof(uint32_t));

		return Helper.uint;
	}

	// Expands secret key + salt to fill the Expanded key (and all sub keys).
	// Can be optimized with a state preserving hasher expansion.
	void SetKeyWithSalt(uint8_t* secretKey, const uint32_t salt)
	{
		uint8_t ByteCount = 0;

		uint8_t Rounds = 1;
		uint8_t Chunk = 0;

		ArrayToUint32 HasherHelper;

		while (ByteCount < sizeof(ExpandedKeySource))
		{
			Hasher.reset();

			// Secret key.
			Hasher.update(secretKey, KeySize);

			// Salt.
			// We resalt Round times because we're stretching the original hash state,
			// without directly restoring the state.
			// Fortunately, our hash is fast.
			for (uint8_t i = 0; i < Rounds; i++)
			{
				HasherHelper.uint = salt;
				Hasher.update(HasherHelper.array, sizeof(uint32_t));
			}

			Chunk = min((uint8_t)sizeof(ExpandedKeySource) - ByteCount, (uint8_t)Hasher.hashSize());

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

		// Update seeds for token generators.
		HasherHelper.array[0] = ExpandedKeySource.Keys.CryptoSeed[0];
		HasherHelper.array[1] = ExpandedKeySource.Keys.CryptoSeed[1];
		HasherHelper.array[2] = ExpandedKeySource.Keys.CryptoSeed[2];
		HasherHelper.array[3] = ExpandedKeySource.Keys.CryptoSeed[3];

		LinkTokenGenerator.SetSeed(HasherHelper.uint);

		HasherHelper.array[0] = ExpandedKeySource.Keys.ChannelSeed[0];
		HasherHelper.array[1] = ExpandedKeySource.Keys.ChannelSeed[1];
		HasherHelper.array[2] = ExpandedKeySource.Keys.ChannelSeed[2];
		HasherHelper.array[3] = ExpandedKeySource.Keys.ChannelSeed[3];

		ChannelTokenGenerator.SetSeed(HasherHelper.uint);
	}

private:
	void ResetCypherBlock()
	{
		Cypher.setKey(ExpandedKeySource.Keys.Key, KeySize);
		Cypher.setIV(ExpandedKeySource.Keys.IV, IVSize);
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
