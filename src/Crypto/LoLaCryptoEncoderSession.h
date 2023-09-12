// LoLaCryptoEncoderSession.h

#ifndef _LOLA_CRYPTO_ENCODER_SESSION_h
#define _LOLA_CRYPTO_ENCODER_SESSION_h

#include "LoLaCryptoSession.h"
#include "..\Clock\Timestamp.h"

/*
* https://github.com/rweather/arduinolibs
*/
#include <ChaCha.h>

/// <summary>
/// Encodes and decodes LoLa packets.
/// </summary>
class LoLaCryptoEncoderSession : public LoLaCryptoSession
{
private:
	ChaCha CryptoCypher; // Cryptographic cypher.

private:
	uint8_t MatchMac[LoLaPacketDefinition::MAC_SIZE]{};

public:
	/// <summary>
	/// </summary>
	/// <param name="expandedKey">sizeof = LoLaLinkDefinition::HKDFSize</param>
	/// <param name="accessPassword">sizeof = LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE</param>
	LoLaCryptoEncoderSession(LoLaLinkDefinition::ExpandedKeyStruct* expandedKey, const uint8_t* accessPassword)
		: LoLaCryptoSession(expandedKey, accessPassword)
		, CryptoCypher(LoLaCryptoDefinition::CYPHER_ROUNDS)
	{
	}

	virtual const bool Setup()
	{
		return
			LoLaCryptoSession::Setup() &&
			CryptoCypher.keySize() == LoLaCryptoDefinition::CYPHER_KEY_SIZE &&
			CryptoCypher.ivSize() == LoLaCryptoDefinition::CYPHER_IV_SIZE &&
			LoLaCryptoDefinition::MAC_KEY_SIZE >= LoLaCryptoDefinition::CYPHER_TAG_SIZE;
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="inPacket"></param>
	/// <param name="data"></param>
	/// <param name="timestamp"></param>
	/// <param name="dataSize"></param>
	/// <param name="counter"></param>
	/// <returns>True if packet was accepted.</returns>
	const bool DecodeInPacket(const uint8_t* inPacket, uint8_t* data, Timestamp& timestamp, uint8_t& counter, const uint8_t dataSize)
	{
		const uint16_t tokenRoll = timestamp.GetRollingMicros() / LoLaLinkDefinition::SUB_TOKEN_PERIOD_MICROS;

		// Set packet authentication tag.
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = ExpandedKey->IdKey[0] ^ ((uint8_t)timestamp.Seconds) ^ inPacket[LoLaPacketDefinition::ID_INDEX];
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_SIZE_INDEX] = dataSize;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = (uint8_t)(tokenRoll);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = (uint8_t)(tokenRoll >> 8);

		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = (uint8_t)(timestamp.Seconds);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = (uint8_t)(timestamp.Seconds >> 8);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = (uint8_t)(timestamp.Seconds >> 16);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = (uint8_t)(timestamp.Seconds >> 24);

		for (uint_fast8_t i = LoLaCryptoDefinition::CYPHER_TAG_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = 0;
		}

		/*****************/
		// Start HMAC with 16 byte Auth Key.
		CryptoHasher.reset(ExpandedKey->MacKey);

		// Add upper nonce to hash content, as it is discarded on finalization truncation.
		CryptoHasher.update(&Nonce[LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE], LoLaCryptoDefinition::CYPHER_TAG_SIZE - LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE);

		// Data.
		CryptoHasher.update(&inPacket[LoLaPacketDefinition::DATA_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Implicit addressing.
		CryptoHasher.update(InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);

		// HMAC finalized with short nonce and copied to MatchMac.
		// Only the first MAC_SIZE bytes are effectively used.
		CryptoHasher.finalize(Nonce, MatchMac, LoLaPacketDefinition::MAC_SIZE);

		// Clear hasher from sensitive material. Disabled for performance.
		//CryptoHasher.clear();
		/*****************/

		/*****************/
		// Reject if plaintext MAC from packet mismatches.
		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAC_SIZE; i++)
		{
			if (MatchMac[i] != inPacket[LoLaPacketDefinition::MAC_INDEX + i])
			{
				// Packet rejected.
				return false;
			}
		}
		/*****************/

		/*****************/
		// Write back the encrypted counter from the packet id.
		counter = Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX];

		// Reset entropy.
		CryptoCypher.setKey(ExpandedKey->CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);
		CryptoCypher.setIV(ExpandedKey->CypherIv, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Set the cypher counter with the Nonce.
		CryptoCypher.setCounter(Nonce, LoLaCryptoDefinition::CYPHER_TAG_SIZE);

		// Decrypt everything but the packet id.
		CryptoCypher.decrypt(data, &inPacket[LoLaPacketDefinition::DATA_INDEX], dataSize);

		// Clear cypher from sensitive material. Disabled for performance.
		//CryptoCypher.clear();
		/*****************/

		return true;
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	/// <returns>True if packet was accepted.</returns>
	const bool DecodeInPacket(const uint8_t* inPacket, uint8_t* data, uint8_t& counter, const uint8_t dataSize)
	{
		// Set clear authentication tag.
		FillNonceClear();

		// Calculate MAC from content.
		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(&inPacket[LoLaPacketDefinition::ID_INDEX], sizeof(uint8_t));
		CryptoHasher.update(&dataSize, sizeof(uint8_t));
		CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
		CryptoHasher.update(&inPacket[LoLaPacketDefinition::CONTENT_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Only the first MAC_SIZE bytes are effectively used.
		CryptoHasher.finalize(Nonce, MatchMac, LoLaPacketDefinition::MAC_SIZE);

		// Clear hasher from sensitive material. Disabled for performance.
		//CryptoHasher.clear();

		// Reject if HMAC mismatches plaintext MAC from packet.
		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAC_SIZE; i++)
		{
			if (MatchMac[i] != inPacket[LoLaPacketDefinition::MAC_INDEX + i])
			{
				// Packet rejected.
				return false;
			}
		}

		// Copy plaintext counter.
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

		// Set clear authentication tag.
		FillNonceClear();

		// Set HMAC without implicit addressing, key or token.
		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(&counter, sizeof(uint8_t));
		CryptoHasher.update(&dataSize, sizeof(uint8_t));
		CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
		CryptoHasher.update(&outPacket[LoLaPacketDefinition::CONTENT_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Only the first LoLaPacketDefinition:MAC_SIZE bytes are effectively used.
		CryptoHasher.finalize(Nonce, &outPacket[LoLaPacketDefinition::MAC_INDEX], LoLaPacketDefinition::MAC_SIZE);

		// Clear hasher from sensitive material. Disabled for performance.
		//CryptoHasher.clear();
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="data"></param>
	/// <param name="outPacket"></param>
	/// <param name="timestamp"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	void EncodeOutPacket(const uint8_t* data, uint8_t* outPacket, Timestamp& timestamp, const uint8_t counter, const uint8_t dataSize)
	{
		const uint16_t tokenRoll = timestamp.GetRollingMicros() / LoLaLinkDefinition::SUB_TOKEN_PERIOD_MICROS;

		// Set packet authentication tag.
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = counter;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_SIZE_INDEX] = dataSize;
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = (uint8_t)(tokenRoll);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = (uint8_t)(tokenRoll >> 8);

		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = (uint8_t)(timestamp.Seconds);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = (uint8_t)(timestamp.Seconds >> 8);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = (uint8_t)(timestamp.Seconds >> 16);
		Nonce[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = (uint8_t)(timestamp.Seconds >> 24);

		for (uint_fast8_t i = LoLaCryptoDefinition::CYPHER_TAG_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = 0;
		}

		/*****************/
		// Reset entropy.
		CryptoCypher.setKey(ExpandedKey->CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);
		CryptoCypher.setIV(ExpandedKey->CypherIv, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Set cypher counter with mixed cypher counter for packet data.
		CryptoCypher.setCounter(Nonce, LoLaCryptoDefinition::CYPHER_TAG_SIZE);

		// Encrypt the everything but the packet id.
		CryptoCypher.encrypt(&outPacket[LoLaPacketDefinition::DATA_INDEX], data, dataSize);

		// Clear cypher from sensitive material. Disabled for performance.
		//CryptoCypher.clear();

		// Write encrypted counter as packet id.
		outPacket[LoLaPacketDefinition::ID_INDEX] = ExpandedKey->IdKey[0] ^ ((uint8_t)timestamp.Seconds) ^ counter;
		/*****************/

		/*****************/
		// Start HMAC.
		CryptoHasher.reset(ExpandedKey->MacKey);

		// Add upper nonce to hash content, as it is discarded on finalization truncation.
		CryptoHasher.update(&Nonce[LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE], LoLaCryptoDefinition::CYPHER_TAG_SIZE - LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE);

		// Data.
		CryptoHasher.update(&outPacket[LoLaPacketDefinition::DATA_INDEX],
			LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Implicit addressing.
		CryptoHasher.update(OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);

		// HMAC finalized with short nonce and copied to packet.
		// Only the first LoLaPacketDefinition:MAC_SIZE bytes are effectively used.
		CryptoHasher.finalize(Nonce, &outPacket[LoLaPacketDefinition::MAC_INDEX], LoLaPacketDefinition::MAC_SIZE);

		// Clear hasher from sensitive material. Disabled for performance.
		//CryptoHasher.clear();
		/*****************/
	}
};
#endif