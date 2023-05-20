// LoLaCryptoEncoderSession.h

#ifndef _LOLA_CRYPTO_ENCODER_SESSION_h
#define _LOLA_CRYPTO_ENCODER_SESSION_h

#include "LoLaCryptoPrimitives.h"
#include "LoLaCryptoPkeSession.h"

class LoLaCryptoEncoderSession : public LoLaCryptoPkeSession
{
private:
	LoLaCryptoPrimitives::CypherType CryptoCypher; // Cryptographic cypher.
	LoLaCryptoPrimitives::MacHashType CryptoHasher; // HMAC hasher.

private:
	/// <summary>
	/// Authentication Tag with mixed usage.
	///  - counter for Cypher.
	///  - IV and AuthTag for HMAC.
	/// </summary>
	uint8_t AuthTag[LoLaCryptoDefinition::MAC_KEY_SIZE]{};
	uint8_t MatchMac[LoLaPacketDefinition::MAC_SIZE]{};

public:
	LoLaCryptoEncoderSession()
		: LoLaCryptoPkeSession()
		, CryptoHasher()
		, CryptoCypher(LoLaCryptoDefinition::CYPHER_ROUNDS)
	{
		// Set tag zero values, above the tag size.
		for (uint_fast8_t i = LoLaCryptoDefinition::CYPHER_TAG_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			AuthTag[i] = 0;
		}
	}

	virtual const bool Setup()
	{
		return LoLaCryptoPkeSession::Setup() &&
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
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = ExpandedKey.IdKey[0] ^ ((uint8_t)tokenRoll) ^ inPacket[LoLaPacketDefinition::ID_INDEX];
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_SIZE_INDEX] = dataSize;
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = (uint8_t)(tokenRoll);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = (uint8_t)(tokenRoll >> 8);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = (uint8_t)(timestamp.Seconds);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = (uint8_t)(timestamp.Seconds >> 8);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = (uint8_t)(timestamp.Seconds >> 16);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = (uint8_t)(timestamp.Seconds >> 24);


		/*****************/
		// Start HMAC with 16 byte Auth Key.
		CryptoHasher.reset(ExpandedKey.MacKey);

		// Add upper nonce to hash content, as it is discarded on finalization truncation.
		CryptoHasher.update(&AuthTag[LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE], LoLaCryptoDefinition::CYPHER_TAG_SIZE - LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE);

		// Data.
		CryptoHasher.update(&inPacket[LoLaPacketDefinition::DATA_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Implicit addressing.
		CryptoHasher.update(InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);

		// HMAC finalized with short nonce and copied to MatchMac.
		// Only the first MAC_SIZE bytes are effectively used.
		CryptoHasher.finalize(AuthTag, MatchMac, LoLaPacketDefinition::MAC_SIZE);

		// Clear hasher from sensitive material. Disabled for performance.
		//CryptoHasher->clear();
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
		counter = AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX];

		// Reset entropy.
		CryptoCypher.setKey(ExpandedKey.CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);
		CryptoCypher.setIV(ExpandedKey.CypherIv, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Set the cypher counter with the AuthTag.
		CryptoCypher.setCounter(AuthTag, LoLaCryptoDefinition::CYPHER_TAG_SIZE);

		// Decrypt everything but the packet id.
		CryptoCypher.decrypt(data, &inPacket[LoLaPacketDefinition::DATA_INDEX], dataSize);

		// Clear cypher from sensitive material. Disabled for performance.
		// CryptoCypher->clear();
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
		// Calculate MAC from content.
		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(&inPacket[LoLaPacketDefinition::CONTENT_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));
		CryptoHasher.finalize(EmptyKey, MatchMac, LoLaPacketDefinition::MAC_SIZE);

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
	/// 
	/// </summary>
	/// <param name="data"></param>
	/// <param name="outPacket"></param>
	/// <param name="counter"></param>
	/// <param name="dataSize"></param>
	void EncodeOutPacket(const uint8_t* data, uint8_t* outPacket, const uint8_t counter, const uint8_t dataSize)
	{
		// Plaintext copy of data to output.
		for (uint_fast8_t i = 0; i < dataSize; i++)
		{
			outPacket[i + LoLaPacketDefinition::DATA_INDEX] = data[i];
		}

		// Set rolling plaintext rolling counter id.
		outPacket[LoLaPacketDefinition::ID_INDEX] = counter;

		// Set HMAC without implicit addressing or token.
		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(&outPacket[LoLaPacketDefinition::CONTENT_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));
		CryptoHasher.finalize(EmptyKey, &outPacket[LoLaPacketDefinition::MAC_INDEX], LoLaPacketDefinition::MAC_SIZE);
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
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = counter;
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_SIZE_INDEX] = dataSize;
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = (uint8_t)(tokenRoll);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = (uint8_t)(tokenRoll >> 8);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = (uint8_t)(timestamp.Seconds);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = (uint8_t)(timestamp.Seconds >> 8);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = (uint8_t)(timestamp.Seconds >> 16);
		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = (uint8_t)(timestamp.Seconds >> 24);


		/*****************/	
		// Reset entropy.
		CryptoCypher.setKey(ExpandedKey.CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);
		CryptoCypher.setIV(ExpandedKey.CypherIv, LoLaCryptoDefinition::CYPHER_IV_SIZE);

		// Set cypher counter with mixed cypher counter for packet data.
		CryptoCypher.setCounter(AuthTag, LoLaCryptoDefinition::CYPHER_TAG_SIZE);

		// Encrypt the everything but the packet id.
		CryptoCypher.encrypt(&outPacket[LoLaPacketDefinition::DATA_INDEX], data, dataSize);

		// Clear cypher from sensitive material. Disabled for performance.
		// CryptoCypher->clear();

		// Write encrypted counter as packet id.
		outPacket[LoLaPacketDefinition::ID_INDEX] = ExpandedKey.IdKey[0] ^ ((uint8_t)tokenRoll) ^ AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX];
		/*****************/

		/*****************/
		// Start HMAC.
		CryptoHasher.reset(ExpandedKey.MacKey);

		// Add upper nonce to hash content, as it is discarded on finalization truncation.
		CryptoHasher.update(&AuthTag[LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE], LoLaCryptoDefinition::CYPHER_TAG_SIZE - LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE);

		// Data.
		CryptoHasher.update(&outPacket[LoLaPacketDefinition::DATA_INDEX],
			LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));

		// Implicit addressing.
		CryptoHasher.update(OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);

		// HMAC finalized with short nonce and copied to packet.
		// Only the first LoLaPacketDefinition:MAC_SIZE bytes are effectively used.
		CryptoHasher.finalize(AuthTag, &outPacket[LoLaPacketDefinition::MAC_INDEX], LoLaPacketDefinition::MAC_SIZE);

		// Clear hasher from sensitive material. Disabled for performance.
		//CryptoHasher->clear();
		/*****************/
	}
};
#endif