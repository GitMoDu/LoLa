// LoLaCryptoDefinition.h

#ifndef _LOLA_CRYPTO_DEFINITION_h
#define _LOLA_CRYPTO_DEFINITION_h

#include <stdint.h>

class LoLaCryptoDefinition
{
#pragma region Public Key Exchange
public:
	/// <summary>
	/// Elliptic-curve Diffie–Hellman public key exchange.
	/// SECP 160 R1
	/// Depends on https://github.com/kmackay/micro-ecc .
	/// </summary>
	//using uECC_secp160r1

	/// <summary>
	/// Public key.
	/// Uncompressed size is 40 bytes.
	/// </summary>
	static constexpr uint8_t PUBLIC_KEY_SIZE = 40;

	/// <summary>
	/// Compressed Public key to be exchanged.
	/// 161 bits take 21 bytes.
	/// </summary>
	static constexpr uint8_t COMPRESSED_KEY_SIZE = 21;

	/// <summary>
	/// Private key size.
	/// 161 bits take 21 bytes.
	/// </summary>
	static constexpr uint8_t PRIVATE_KEY_SIZE = COMPRESSED_KEY_SIZE;

#pragma endregion
#pragma region Encryption
public:
	/// <summary>
	/// Blake2S (Cryptographic Hash function) - secret key expansion using HKDF.
	/// Up to 32 bytes of output.
	/// Not found to patentable.
	/// </summary>
	/// </summary>
	//using KeyHashType = BLAKE2s

	/// Chacha8 256 (Cryptographic Cypher function) - message encoding/encryption.
	/// 32 byte (256 bit) cypher key.
	/// 8 Byte IV, Counter with the same size, according to specification.
	/// Public domain.
	/// </summary>
	/// using MacHashType = Poly1305
	static constexpr uint8_t CYPHER_KEY_SIZE = 32;
	static constexpr uint8_t CYPHER_IV_SIZE = 8;
	static constexpr uint8_t CYPHER_ROUNDS = 20;

#pragma endregion
#pragma region Integrity and Authenticity
public:
	/// <summary>
	/// Poly1305 (Cryptographic Message Authentication function) - verify the data integrity and the authenticity.
	/// Up to 16 bytes of output.
	/// Not found to be patentable.
	/// </summary>
	/// using MacHashType = Poly1305
	static constexpr uint8_t MAC_KEY_SIZE = 16;
	static constexpr uint8_t NONCE_SIZE = 16;

	/// <summary>
	/// CRC-32/ISO-HDLC (Fast Hash function) - non-cryptographic, fast 32-bit pseudo-hasher.
	/// HDLC is attested and defined in ISO / IEC 13239.
	/// Alias: CRC-32, CRC-32/ADCCP, CRC-32/V-42, CRC-32/XZ, PKZIP
	/// </summary>
	//using FastHashType = FastCRC32

	/// <summary>
	/// Size of time-based token.
	/// 32 bits cover 1 Unix epoch, since token is updated every second.
	/// </summary>
	static constexpr uint8_t TIME_TOKEN_KEY_SIZE = 4;

	/// <summary>
	/// Size of sub second token.
	/// 16 bit rolling sub-timestamp token.
	/// </summary>
	static constexpr uint8_t TIME_SUB_TOKEN_KEY_SIZE = 2;

	/// <summary>
	/// Size of encoded rolling counter.
	/// 8 bit rolling token.
	/// </summary>
	static constexpr uint8_t PACKET_ID_SIZE = 1;

	/// <summary>
	/// MAC is truncated to 32 bits, so any nonce value above doesn't affect the HMAC result often.
	/// </summary>
	static constexpr uint8_t NONCE_EFFECTIVE_SIZE = 4;

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
public:
	static constexpr uint8_t CYPHER_TAG_ID_INDEX = 0;
	static constexpr uint8_t CYPHER_TAG_ID_SIZE = 1;
	static constexpr uint8_t CYPHER_TAG_SIZE_INDEX = CYPHER_TAG_ID_INDEX + PACKET_ID_SIZE;
	static constexpr uint8_t CYPHER_TAG_ROLL_INDEX = CYPHER_TAG_SIZE_INDEX + CYPHER_TAG_ID_SIZE;
	static constexpr uint8_t CYPHER_TAG_TIMESTAMP_INDEX = CYPHER_TAG_ROLL_INDEX + TIME_SUB_TOKEN_KEY_SIZE;
	static constexpr uint8_t CYPHER_TAG_SIZE = CYPHER_TAG_TIMESTAMP_INDEX + TIME_TOKEN_KEY_SIZE + 1;
#pragma endregion
};

static const uint8_t EmptyKey[LoLaCryptoDefinition::MAC_KEY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t CounterZero[1] = { 0 };
#endif