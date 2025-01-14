// LoLaCryptoEncoderSession.h

#ifndef _LOLA_CRYPTO_ENCODER_SESSION_h
#define _LOLA_CRYPTO_ENCODER_SESSION_h

#include "LoLaCryptoSession.h"

/*
* https://github.com/rweather/arduinolibs
*
* Requires library CryptoLW from repository.
*/
#include <Ascon128.h>

/// <summary>
/// Encodes and decodes LoLa packets.
/// </summary>
class LoLaCryptoEncoderSession : public LoLaCryptoSession
{
private:
	/// <summary>
	/// Cryptographic cypher.
	/// </summary>
	Ascon128 CryptoCypher{};

public:
	LoLaCryptoEncoderSession()
		: LoLaCryptoSession()
	{}

	const bool Setup()
	{
		return CryptoHasher.DIGEST_LENGTH >= LoLaPacketDefinition::MAC_SIZE
			&& 2 == LoLaPacketDefinition::ID_SIZE
			&& CryptoCypher.keySize() == LoLaCryptoDefinition::CYPHER_KEY_SIZE
			&& CryptoCypher.ivSize() == LoLaCryptoDefinition::CYPHER_IV_SIZE;
	}

	/// <summary>
	/// Validates and decodes packet with implicit addressing, key and token.
	/// </summary>
	/// <param name="inPacket"></param>
	/// <param name="data"></param>
	/// <param name="timestamp"></param>
	/// <param name="dataSize"></param>
	/// <param name="counter"></param>
	/// <returns>True if packet was accepted.</returns>
	const bool DecodeInPacket(const uint8_t* inPacket, uint8_t* data, const uint32_t timestamp, uint16_t& counter, const uint8_t dataSize)
	{
		// Set Nonce.
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = timestamp;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = timestamp >> 8;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = timestamp >> 16;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = timestamp >> 24;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id];
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX + 1] = inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id + 1];
		memcpy(&Nonce[LoLaCryptoDefinition::CYPHER_TAG_SIZE], InputKey, LoLaCryptoDefinition::CYPHER_IV_SIZE - LoLaCryptoDefinition::CYPHER_TAG_SIZE);

		/*****************/
#if defined(LOLA_USE_POLY1305)
		// Start HMAC with Key.
		CryptoHasher.reset(ExpandedKey.MacKey);
#else
		// Start MAC.
		CryptoHasher.reset();

		// Nonce.
		CryptoHasher.update(Nonce, LoLaCryptoDefinition::CYPHER_IV_SIZE);
#endif

		// Content.
		CryptoHasher.update(&inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Content], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Reject if HMAC mismatches plaintext MAC from packet.
#if defined(LOLA_USE_POLY1305)
		if (!CryptoHasher.macMatches(Nonce, &inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Mac]))
#else
		if (!CryptoHasher.macMatches(&inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Mac]))
#endif
		{
			// Packet rejected.
			return false;
		}
		/*****************/

		/*****************/
		// Write back the counter from the packet id.
		counter = ((uint16_t)inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id + 1] << 8) | inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id];

		// Reset Key entropy.
		CryptoCypher.setKey(ExpandedKey.CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);

		// Set cypher IV with mixed nonce as authentication data.
		CryptoCypher.setIV(Nonce, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Decrypt everything but the packet id.
		CryptoCypher.decrypt(data, &inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Data], dataSize);
		/*****************/

		return true;
	}

	/// <summary>
	/// Validates and decodes packet without implicit addressing, key or token.
	/// </summary>
	/// <param name="inPacket"></param>
	/// <param name="data"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	/// <returns>True if packet was accepted.</returns>
	const bool DecodeInPacket(const uint8_t* inPacket, uint8_t* data, uint16_t& counter, const uint8_t dataSize)
	{
		// Calculate MAC from content.
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
		CryptoHasher.update(&inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Content], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Reject if HMAC mismatches plaintext MAC from packet.
#if defined(LOLA_USE_POLY1305)
		if (!CryptoHasher.macMatches(Nonce, &inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Mac]))
#else
		if (!CryptoHasher.macMatches(&inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Mac]))
#endif
		{
			// Packet rejected.
			return false;
		}

		// Copy plaintext counter from packet id.		
		counter = ((uint16_t)inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id + 1] << 8) | inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id];

		// Copy plaintext content to in data.
		memcpy(data, &inPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Data], dataSize);

		return true;
	}

	/// <summary>
	/// Encodes packet without implicit addressing, key or token.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="outPacket"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	void EncodeOutPacket(const uint8_t* data, uint8_t* outPacket, const uint16_t counter, const uint8_t dataSize)
	{
		// Set rolling plaintext rolling counter id.
		outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id] = counter;
		outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id + 1] = counter >> 8;

		// Plaintext copy of data to output.
		memcpy(&outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Data], data, dataSize);

		// Set HMAC without implicit addressing, key or token.
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
		CryptoHasher.update(&outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Content], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Only the first LoLaPacketDefinition:MAC_SIZE bytes are effectively used.
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, &outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Mac], LoLaPacketDefinition::MAC_SIZE);
#else
		CryptoHasher.finalize(&outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Mac], LoLaPacketDefinition::MAC_SIZE);
#endif
	}

	/// <summary>
	/// Encodes packet with implicit addressing, key and token.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="outPacket"></param>
	/// <param name="timestamp"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	void EncodeOutPacket(const uint8_t* data, uint8_t* outPacket, const uint32_t timestamp, const uint16_t counter, const uint8_t dataSize)
	{
		//// Set Nonce.
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = timestamp;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = timestamp >> 8;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = timestamp >> 16;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = timestamp >> 24;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = counter;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX + 1] = counter >> 8;
		memcpy(&Nonce[LoLaCryptoDefinition::CYPHER_TAG_SIZE], OutputKey, LoLaCryptoDefinition::CYPHER_IV_SIZE - LoLaCryptoDefinition::CYPHER_TAG_SIZE);

		/*****************/
		// Reset Key entropy.
		CryptoCypher.setKey(ExpandedKey.CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);

		// Set cypher IV with nonce as authentication data.
		CryptoCypher.setIV(Nonce, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Encrypt data to packet.
		CryptoCypher.encrypt(&outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Data], data, dataSize);

		// Copy plaintext counter to packet id.		
		outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id] = Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX];
		outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Id + 1] = Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX + 1];
		/*****************/

		/*****************/
#if defined(LOLA_USE_POLY1305)
		// Start HMAC with Key.
		CryptoHasher.reset(ExpandedKey.MacKey);
#else
		// Start MAC.
		CryptoHasher.reset();

		// Nonce.
		CryptoHasher.update(Nonce, LoLaCryptoDefinition::CYPHER_IV_SIZE);
#endif

		// Content.
		CryptoHasher.update(&outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Content], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

#if defined(LOLA_USE_POLY1305)
		// Nonce and finalize HMAC.
		CryptoHasher.finalize(Nonce, &outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Mac], LoLaPacketDefinition::MAC_SIZE);
#else
		// Finalize MAC.
		CryptoHasher.finalize(&outPacket[(uint8_t)LoLaPacketDefinition::IndexEnum::Mac], LoLaPacketDefinition::MAC_SIZE);
#endif
		/*****************/
	}
};
#endif