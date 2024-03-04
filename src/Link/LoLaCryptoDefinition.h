// LoLaCryptoDefinition.h

#ifndef _LOLA_CRYPTO_DEFINITION_h
#define _LOLA_CRYPTO_DEFINITION_h

#include <stdint.h>
#include "..\Link\LoLaPacketDefinition.h"
#include "..\Link\LoLaLinkDefinition.h"

namespace LoLaCryptoDefinition
{
	enum class MacType : uint8_t
	{
		Xoodyak = 0x88,
		Poly1305 = 0x13
	};

	/// Ascon (Cryptographic Cypher function).
	/// Public domain.
	/// 16 byte (128 bit) cypher key.
	/// </summary>
	static constexpr uint8_t CYPHER_KEY_SIZE = 16;
	static constexpr uint8_t CYPHER_IV_SIZE = 16;

#if defined(LOLA_USE_POLY1305)
	/// Poly1305 (Cryptographic Hash function).
	/// MIT licensed.
	/// 16 byte (128 bit) bit digest.
	/// </summary>
#else
	/// Xoodyak (Cryptographic Hash function).
	/// Creative Commons Attribution 4.0 International License.
	/// 32 byte (256 bit) bit digest.
	/// </summary>
#endif

	/// <summary>
	/// Channel PRNG entropy/key size.
	/// </summary>
	static constexpr uint8_t CHANNEL_KEY_SIZE = 4;

	////////// Public Key Exchange
	/// <summary>
	/// Elliptic-curve Diffie–Hellman public key exchange.
	/// SECP 160 R1
	/// </summary>
	static constexpr uint8_t PKE_CURVE_SIZE = 160;

	/// <summary>
	/// Shared secret key size in bytes.
	/// </summary>
	static constexpr uint8_t SHARED_KEY_SIZE = PKE_CURVE_SIZE / 8;

	/// <summary>
	/// Public key, uncompressed.
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

	/// <summary>
	/// Implicit addressing key size.
	/// </summary>
	static constexpr uint8_t ADDRESS_KEY_SIZE = CYPHER_IV_SIZE - CYPHER_TAG_SIZE;

	/// <summary>
	/// The raw keys that are used during runtime encryption.
	/// These are filled in with HKDF from the shared secret key.
	/// </summary>
	struct ExpandedKeyStruct
	{
		/// <summary>
		/// Cypher Key.
		/// </summary>
		uint8_t CypherKey[CYPHER_KEY_SIZE];

		/// <summary>
		/// Cypher Iv.
		/// </summary>
		uint8_t CypherIvSeed[ADDRESS_KEY_SIZE];

#if defined(LOLA_USE_POLY1305)
		/// <summary>
		/// MAC key for Poly1305 (16 bytes).
		/// </summary>
		uint8_t MacKey[CYPHER_IV_SIZE];
#endif

		/// <summary>
		/// Channel PRNG seed.
		/// </summary>
		uint8_t ChannelSeed[CHANNEL_KEY_SIZE];
	};

	static constexpr uint8_t HKDFSize = sizeof(ExpandedKeyStruct);
};
#endif