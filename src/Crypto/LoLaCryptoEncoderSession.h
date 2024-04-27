// LoLaCryptoEncoderSession.h

#ifndef _LOLA_CRYPTO_ENCODER_SESSION_h
#define _LOLA_CRYPTO_ENCODER_SESSION_h

#include "LoLaCryptoSession.h"

/*
* https://github.com/rweather/arduinolibs
*
* Requires libraries Crypto and CryptoLW from repository.
*/
#if defined(LOLA_USE_POLY1305)
#include <Poly1305.h>
#else
#include <Ascon128.h>
#endif

/// <summary>
/// Encodes and decodes LoLa packets.
/// </summary>
class LoLaCryptoEncoderSession : public LoLaCryptoSession
{
private:
	Ascon128 CryptoCypher{}; // Cryptographic cypher.

public:
	/// <summary>
	/// </summary>
	/// <param name="accessPassword">sizeof = LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE</param>
	LoLaCryptoEncoderSession(const uint8_t accessPassword[ACCESS_CONTROL_PASSWORD_SIZE])
		: LoLaCryptoSession(accessPassword)
	{}

	virtual const bool Setup()
	{
		return LoLaCryptoSession::Setup()
			&& 2 == ID_SIZE
			&& CryptoCypher.keySize() == CYPHER_KEY_SIZE
			&& CryptoCypher.ivSize() == CYPHER_IV_SIZE;
	}

	/// <summary>
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
		Nonce[CYPHER_TAG_TIMESTAMP_INDEX] = timestamp;
		Nonce[CYPHER_TAG_TIMESTAMP_INDEX + 1] = timestamp >> 8;
		Nonce[CYPHER_TAG_TIMESTAMP_INDEX + 2] = timestamp >> 16;
		Nonce[CYPHER_TAG_TIMESTAMP_INDEX + 3] = timestamp >> 24;
		Nonce[CYPHER_TAG_ID_INDEX] = inPacket[ID_INDEX];
		Nonce[CYPHER_TAG_ID_INDEX + 1] = inPacket[ID_INDEX + 1];
		memcpy(&Nonce[CYPHER_TAG_SIZE], InputKey, CYPHER_IV_SIZE - CYPHER_TAG_SIZE);

		/*****************/
#if defined(LOLA_USE_POLY1305)
		// Start HMAC with Key.
		CryptoHasher.reset(ExpandedKey.MacKey);
#else
		// Start MAC.
		CryptoHasher.reset();

		// Nonce.
		CryptoHasher.update(Nonce, CYPHER_IV_SIZE);
#endif

		// Content.
		CryptoHasher.update(&inPacket[CONTENT_INDEX], GetContentSizeFromDataSize(dataSize));

		// Reject if HMAC mismatches plaintext MAC from packet.
#if defined(LOLA_USE_POLY1305)
		if (!CryptoHasher.macMatches(Nonce, &inPacket[MAC_INDEX]))
#else
		if (!CryptoHasher.macMatches(&inPacket[MAC_INDEX]))
#endif
		{
			// Packet rejected.
			return false;
		}
		/*****************/

		/*****************/
		// Write back the counter from the packet id.
		counter = ((uint16_t)inPacket[ID_INDEX + 1] << 8) | inPacket[ID_INDEX];

		// Reset Key entropy.
		CryptoCypher.setKey(ExpandedKey.CypherKey, CYPHER_KEY_SIZE);

		// Set cypher IV with mixed nonce as authentication data.
		CryptoCypher.setIV(Nonce, CYPHER_IV_SIZE);

		// Decrypt everything but the packet id.
		CryptoCypher.decrypt(data, &inPacket[DATA_INDEX], dataSize);
		/*****************/

		return true;
	}

	/// <summary>
	/// </summary>
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
		CryptoHasher.update(ProtocolId, PROTOCOL_ID_SIZE);
		CryptoHasher.update(&inPacket[CONTENT_INDEX], GetContentSizeFromDataSize(dataSize));

		// Reject if HMAC mismatches plaintext MAC from packet.
#if defined(LOLA_USE_POLY1305)
		if (!CryptoHasher.macMatches(Nonce, &inPacket[MAC_INDEX]))
#else
		if (!CryptoHasher.macMatches(&inPacket[MAC_INDEX]))
#endif
		{
			// Packet rejected.
			return false;
		}

		// Copy plaintext counter from packet id.		
		counter = ((uint16_t)inPacket[ID_INDEX + 1] << 8) | inPacket[ID_INDEX];

		// Copy plaintext content to in data.
		for (uint_fast8_t i = 0; i < dataSize; i++)
		{
			data[i] = inPacket[i + DATA_INDEX];
		}

		return true;
	}

	/// <summary>
	/// Encodes HMAC without implicit addressing, key or token.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="outPacket"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	void EncodeOutPacket(const uint8_t* data, uint8_t* outPacket, const uint16_t counter, const uint8_t dataSize)
	{
		// Set rolling plaintext rolling counter id.
		outPacket[ID_INDEX] = counter;
		outPacket[ID_INDEX + 1] = counter >> 8;

		// Plaintext copy of data to output.
		for (uint_fast8_t i = 0; i < dataSize; i++)
		{
			outPacket[i + DATA_INDEX] = data[i];
		}

		// Set HMAC without implicit addressing, key or token.
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(ProtocolId, PROTOCOL_ID_SIZE);
		CryptoHasher.update(&outPacket[CONTENT_INDEX], GetContentSizeFromDataSize(dataSize));

		// Only the first LoLaPacketDefinition:MAC_SIZE bytes are effectively used.
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, &outPacket[MAC_INDEX], MAC_SIZE);
#else
		CryptoHasher.finalize(&outPacket[MAC_INDEX], MAC_SIZE);
#endif
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="data"></param>
	/// <param name="outPacket"></param>
	/// <param name="timestamp"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	void EncodeOutPacket(const uint8_t* data, uint8_t* outPacket, const uint32_t timestamp, const uint16_t counter, const uint8_t dataSize)
	{
		//// Set Nonce.
		Nonce[CYPHER_TAG_TIMESTAMP_INDEX] = timestamp;
		Nonce[CYPHER_TAG_TIMESTAMP_INDEX + 1] = timestamp >> 8;
		Nonce[CYPHER_TAG_TIMESTAMP_INDEX + 2] = timestamp >> 16;
		Nonce[CYPHER_TAG_TIMESTAMP_INDEX + 3] = timestamp >> 24;
		Nonce[CYPHER_TAG_ID_INDEX] = counter;
		Nonce[CYPHER_TAG_ID_INDEX + 1] = counter >> 8;
		memcpy(&Nonce[CYPHER_TAG_SIZE], OutputKey, CYPHER_IV_SIZE - CYPHER_TAG_SIZE);

		/*****************/
		// Reset Key entropy.
		CryptoCypher.setKey(ExpandedKey.CypherKey, CYPHER_KEY_SIZE);

		// Set cypher IV with nonce as authentication data.
		CryptoCypher.setIV(Nonce, CYPHER_IV_SIZE);

		// Encrypt data to packet.
		CryptoCypher.encrypt(&outPacket[DATA_INDEX], data, dataSize);

		// Copy plaintext counter to packet id.		
		outPacket[ID_INDEX] = Nonce[CYPHER_TAG_ID_INDEX];
		outPacket[ID_INDEX + 1] = Nonce[CYPHER_TAG_ID_INDEX + 1];
		/*****************/

		/*****************/
#if defined(LOLA_USE_POLY1305)
		// Start HMAC with Key.
		CryptoHasher.reset(ExpandedKey.MacKey);
#else
		// Start MAC.
		CryptoHasher.reset();

		// Nonce.
		CryptoHasher.update(Nonce, CYPHER_IV_SIZE);
#endif

		// Content.
		CryptoHasher.update(&outPacket[CONTENT_INDEX], GetContentSizeFromDataSize(dataSize));

#if defined(LOLA_USE_POLY1305)
		// Nonce and finalize HMAC.
		CryptoHasher.finalize(Nonce, &outPacket[MAC_INDEX], MAC_SIZE);
#else
		// Finalize MAC.
		CryptoHasher.finalize(&outPacket[MAC_INDEX], MAC_SIZE);
#endif
		/*****************/
	}
};
#endif