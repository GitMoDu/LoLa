// LoLaCryptoEncoderSession.h

#ifndef _LOLA_CRYPTO_ENCODER_SESSION_h
#define _LOLA_CRYPTO_ENCODER_SESSION_h

#include "LoLaCryptoSession.h"

/*
* https://github.com/rweather/arduinolibs
*
* Requires libraries Crypto and CryptoLW from repository.
*/
#include <Ascon128.h>

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
	LoLaCryptoEncoderSession(const uint8_t* accessPassword)
		: LoLaCryptoSession(accessPassword)
	{
	}

	virtual const bool Setup()
	{
		return LoLaCryptoSession::Setup()
			&& CryptoCypher.keySize() == LoLaCryptoDefinition::CYPHER_KEY_SIZE
			&& CryptoCypher.ivSize() == LoLaCryptoDefinition::CYPHER_IV_SIZE;
	}

	/// <summary>
	/// </summary>
	/// <param name="inPacket"></param>
	/// <param name="data"></param>
	/// <param name="timestamp"></param>
	/// <param name="dataSize"></param>
	/// <param name="counter"></param>
	/// <returns>True if packet was accepted.</returns>
	const bool DecodeInPacket(const uint8_t* inPacket, uint8_t* data, const uint32_t timestamp, uint8_t& counter, const uint8_t dataSize)
	{
		// Set packet authentication tag.
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = timestamp;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = (timestamp >> 8);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = (timestamp >> 16);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = (timestamp >> 24);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = inPacket[LoLaPacketDefinition::ID_INDEX];
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_SIZE_INDEX] = dataSize;
		for (uint_fast8_t i = LoLaCryptoDefinition::CYPHER_TAG_SIZE; i < LoLaCryptoDefinition::CYPHER_IV_SIZE; i++)
		{
			Nonce[i] = InputKey[i - LoLaCryptoDefinition::CYPHER_TAG_SIZE];
		}

		/*****************/
		CryptoHasher.reset();

		// Nonce.
		CryptoHasher.update(Nonce, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Data.
		CryptoHasher.update(&inPacket[LoLaPacketDefinition::DATA_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Reject if HMAC mismatches plaintext MAC from packet.
		if (!CryptoHasher.macMatches(&inPacket[LoLaPacketDefinition::MAC_INDEX]))
		{
			// Packet rejected.
			return false;
		}
		/*****************/

		/*****************/
		// Write back the counter from the packet id.
		counter = inPacket[LoLaPacketDefinition::ID_INDEX];

		// Reset Key entropy.
		CryptoCypher.setKey(ExpandedKey.CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);

		// Set cypher IV with mixed nonce as authentication data.
		CryptoCypher.setIV(Nonce, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Decrypt everything but the packet id.
		CryptoCypher.decrypt(data, &inPacket[LoLaPacketDefinition::DATA_INDEX], dataSize);
		/*****************/

		return true;
	}

	/// <summary>
	/// </summary>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	/// <returns>True if packet was accepted.</returns>
	const bool DecodeInPacket(const uint8_t* inPacket, uint8_t* data, uint8_t& counter, const uint8_t dataSize)
	{
		// Calculate MAC from content.
		CryptoHasher.reset();
		CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
		CryptoHasher.update(&inPacket[LoLaPacketDefinition::ID_INDEX], LoLaPacketDefinition::ID_SIZE);
		CryptoHasher.update(&dataSize, sizeof(uint8_t));
		CryptoHasher.update(&inPacket[LoLaPacketDefinition::CONTENT_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Reject if HMAC mismatches plaintext MAC from packet.
		if (!CryptoHasher.macMatches(&inPacket[LoLaPacketDefinition::MAC_INDEX]))
		{
			// Packet rejected.
			return false;
		}

		// Copy plaintext counter from packet id.		
		counter = inPacket[LoLaPacketDefinition::ID_INDEX];

		// Copy plaintext content to in data.
		for (uint_fast8_t i = 0; i < dataSize; i++)
		{
			data[i] = inPacket[i + LoLaPacketDefinition::DATA_INDEX];
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
	void EncodeOutPacket(const uint8_t* data, uint8_t* outPacket, const uint8_t counter, const uint8_t dataSize)
	{
		// Set rolling plaintext rolling counter id.
		outPacket[LoLaPacketDefinition::ID_INDEX] = counter;

		// Plaintext copy of data to output.
		for (uint_fast8_t i = 0; i < dataSize; i++)
		{
			outPacket[i + LoLaPacketDefinition::DATA_INDEX] = data[i];
		}

		// Set HMAC without implicit addressing, key or token.
		CryptoHasher.reset();
		CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
		CryptoHasher.update(&counter, LoLaPacketDefinition::ID_SIZE);
		CryptoHasher.update(&dataSize, sizeof(uint8_t));
		CryptoHasher.update(&outPacket[LoLaPacketDefinition::CONTENT_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Only the first LoLaPacketDefinition:MAC_SIZE bytes are effectively used.
		CryptoHasher.finalize(&outPacket[LoLaPacketDefinition::MAC_INDEX], LoLaPacketDefinition::MAC_SIZE);
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="data"></param>
	/// <param name="outPacket"></param>
	/// <param name="timestamp"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	void EncodeOutPacket(const uint8_t* data, uint8_t* outPacket, const uint32_t timestamp, const uint8_t counter, const uint8_t dataSize)
	{
		//// Set packet authentication tag.
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = timestamp;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = (timestamp >> 8);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = (timestamp >> 16);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = (timestamp >> 24);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = counter;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_SIZE_INDEX] = dataSize;
		for (uint_fast8_t i = LoLaCryptoDefinition::CYPHER_TAG_SIZE; i < LoLaCryptoDefinition::CYPHER_IV_SIZE; i++)
		{
			Nonce[i] = OutputKey[i - LoLaCryptoDefinition::CYPHER_TAG_SIZE];
		}

		/*****************/
		// Reset Key entropy.
		CryptoCypher.setKey(ExpandedKey.CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);

		// Set cypher IV with nonce as authentication data.
		CryptoCypher.setIV(Nonce, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Encrypt data to packet.
		CryptoCypher.encrypt(&outPacket[LoLaPacketDefinition::DATA_INDEX], data, dataSize);

		// Copy plaintext counter to packet id.		
		outPacket[LoLaPacketDefinition::ID_INDEX] = counter;
		/*****************/

		/*****************/
		// Start HMAC.
		CryptoHasher.reset();

		// Nonce.
		CryptoHasher.update(Nonce, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Data.
		CryptoHasher.update(&outPacket[LoLaPacketDefinition::DATA_INDEX],
			LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// HMAC finalized and copied to packet.
		CryptoHasher.finalize(&outPacket[LoLaPacketDefinition::MAC_INDEX],
			LoLaPacketDefinition::MAC_SIZE);
		/*****************/
	}
};
#endif