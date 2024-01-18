// LoLaCryptoDefinition.h

#ifndef _LOLA_CRYPTO_DEFINITION_h
#define _LOLA_CRYPTO_DEFINITION_h

#include <stdint.h>
#include "..\Link\LoLaPacketDefinition.h"

class LoLaCryptoDefinition
{
	// Public Key Exchange
public:
	/// <summary>
	/// Elliptic-curve Diffie–Hellman public key exchange.
	/// SECP 160 R1
	/// </summary>
	static constexpr uint8_t PKE_CURVE_SIZE = 160;

	/// <summary>
	/// Shared secret key size.
	/// </summary>
	static constexpr uint8_t SHARED_KEY_SIZE = PKE_CURVE_SIZE / 8;

	/// <summary>
	/// Public key.
	/// Uncompressed size is 40 bytes.
	/// </summary>
	static constexpr uint8_t PUBLIC_KEY_SIZE = SHARED_KEY_SIZE * 2;

	/// <summary>
	/// Compressed Public key to be exchanged.
	/// 161 bits take 21 bytes.
	/// </summary>
	static constexpr uint8_t COMPRESSED_KEY_SIZE = SHARED_KEY_SIZE + 1;

	/// <summary>
	/// Private key size.
	/// 161 bits take 21 bytes.
	/// </summary>
	static constexpr uint8_t PRIVATE_KEY_SIZE = COMPRESSED_KEY_SIZE;

	// Public Address Exchange
public:
	/// <summary>
	/// Public key equivalent.
	/// </summary>
	static constexpr uint8_t PUBLIC_ADDRESS_SIZE = 8;

	// Encryption
public:
	/// Ascon (Cryptographic Cypher function).
	/// Public domain.
	/// 16 byte (128 bit) cypher key.
	/// </summary>
	static constexpr uint8_t CYPHER_KEY_SIZE = 16;
	static constexpr uint8_t CYPHER_IV_SIZE = 16;

	// Integrity and Authenticity
public:
	/// Xoodyak (Cryptographic Hash function).
	/// Creative Commons Attribution 4.0 International License.
	/// 32 byte (256 bit) bit digest.
	/// </summary>

	/// <summary>
	/// Size of time-based token.
	/// 32 bits cover 1 Unix epoch, since token is updated every second.
	/// </summary>
	static constexpr uint8_t TIME_TOKEN_KEY_SIZE = 4;

	/// <summary>
	/// Random challenge to be solved by the partner,
	/// before granting access to next Linking Step..
	/// Uses a simple hash with the Challenge and ACCESS_CONTROL_PASSWORD,
	/// instead of a slow (but more secure) certificate-signature.
	/// </summary>
	static constexpr uint8_t CHALLENGE_CODE_SIZE = 4;
	static constexpr uint8_t CHALLENGE_SIGNATURE_SIZE = 4;


	/// <summary>
	/// Cypher Tag (Nonce) content indexes.
	/// </summary>
	static constexpr uint8_t CYPHER_TAG_TIMESTAMP_INDEX = 0;
	static constexpr uint8_t CYPHER_TAG_ID_INDEX = CYPHER_TAG_TIMESTAMP_INDEX + TIME_TOKEN_KEY_SIZE;
	static constexpr uint8_t CYPHER_TAG_SIZE = CYPHER_TAG_ID_INDEX + LoLaPacketDefinition::ID_SIZE;
};
#endif