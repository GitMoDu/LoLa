// LoLaCryptoPkeSession.h

#ifndef _LOLA_CRYPTO_PKE_SESSION_h
#define _LOLA_CRYPTO_PKE_SESSION_h

#include "LoLaCryptoPrimitives.h"
#include "LoLaCryptoEncoderSession.h"

/*
* https://github.com/kmackay/micro-ecc
*/
#include <uECC.h>

class LoLaCryptoPkeSession : public LoLaCryptoEncoderSession
{
private:
	enum PkeEnum
	{
		CalculatingSecret,
		CalculatingExpandedKey1,
		CalculatingExpandedKey2,
		CalculatingAddressing,
		CalculatingSessionToken,
		PkeCached
	};

private:
	const uECC_Curve_t* ECC_CURVE; // uECC_secp160r1

private:
	PkeEnum PkeState = PkeEnum::CalculatingSecret;

private:
	uint8_t PartnerPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE]{};

	/// <summary>
	/// Shared secret key.
	/// </summary>
	uint8_t SecretKey[LoLaCryptoDefinition::CYPHER_KEY_SIZE]{};

private:
	const uint8_t* LocalPublicKey = nullptr;
	const uint8_t* LocalPrivateKey = nullptr;

public:
	/// <summary>
	/// </summary>
	/// <param name="expandedKey">sizeof = LoLaLinkDefinition::HKDFSize</param>
	/// <param name="accessPassword">sizeof = LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE</param>
	/// <param name="publicKey">sizeof = LoLaCryptoDefinition::PUBLIC_KEY_SIZE</param>
	/// <param name="privateKey">size of = LoLaCryptoDefinition::PRIVATE_KEY_SIZE</param>
	LoLaCryptoPkeSession(LoLaLinkDefinition::ExpandedKeyStruct* expandedKey,
		const uint8_t* accessPassword,
		const uint8_t* publicKey,
		const uint8_t* privateKey)
		: LoLaCryptoEncoderSession(expandedKey, accessPassword)
		, ECC_CURVE(uECC_secp160r1())
		, LocalPublicKey(publicKey)
		, LocalPrivateKey(privateKey)
	{}

	virtual const bool Setup() final
	{
		return LoLaCryptoEncoderSession::Setup() &&
			LocalPublicKey != nullptr && LocalPrivateKey != nullptr;
	}

	const bool SessionIsCached()
	{
		return PkeState == PkeEnum::PkeCached && SessionTokenMatches(PartnerPublicKey);
	}

	void ResetPke()
	{
		PkeState = PkeEnum::CalculatingSecret;
	}

	/// <summary>
	/// Long operations, cannot be done in line with linking protocol.
	/// </summary>
	/// <returns></returns>
	const bool CalculatePke()
	{
		switch (PkeState)
		{
		case PkeEnum::CalculatingSecret:
			uECC_shared_secret(PartnerPublicKey, LocalPrivateKey, SecretKey, ECC_CURVE);
			PkeState = PkeEnum::CalculatingExpandedKey1;
			break;
		case PkeEnum::CalculatingExpandedKey1:
			SetSecretKey(SecretKey);
			PkeState = PkeEnum::CalculatingExpandedKey2;
			break;
		case PkeEnum::CalculatingExpandedKey2:
			CalculateExpandedKey();
			PkeState = PkeEnum::CalculatingAddressing;
			break;
		case PkeEnum::CalculatingAddressing:
			CalculateSessionAddressing(LocalPublicKey, PartnerPublicKey);
			PkeState = PkeEnum::CalculatingSessionToken;
			break;
		case PkeEnum::CalculatingSessionToken:
			CalculateSessionToken(PartnerPublicKey);
			PkeState = PkeEnum::PkeCached;
			break;
		case PkeEnum::PkeCached:
			return true;
		default:
			break;
		}

		return false;
	}

	const bool PublicKeyCollision()
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::PUBLIC_KEY_SIZE; i++)
		{
			if (LocalPublicKey[i] != PartnerPublicKey[i])
			{
				return false;
			}
		}
		return true;
	}

	/// <summary>
	/// Slow operation, can take a few ms, take care.
	/// </summary>
	/// <param name="compressedPublicKey"></param>
	void DecompressPartnerPublicKeyFrom(const uint8_t* compressedPublicKey)
	{
		uECC_decompress(compressedPublicKey, PartnerPublicKey, ECC_CURVE);
	}

	/// <summary>
	/// Fast operation.
	/// </summary>
	/// <param name="target"></param>
	void CompressPublicKeyTo(uint8_t* target)
	{
		uECC_compress(LocalPublicKey, target, ECC_CURVE);
	}
};
#endif