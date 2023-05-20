// LoLaCryptoPkeSession.h

#ifndef _LOLA_CRYPTO_PKE_SESSION_h
#define _LOLA_CRYPTO_PKE_SESSION_h

#include "LoLaCryptoPrimitives.h"
#include "LoLaCryptoSession.h"

class LoLaCryptoPkeSession : public LoLaCryptoSession
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
	uint8_t PartnerPublicKey[LoLaCryptoDefinition::PUBLIC_KEY_SIZE];
	uint8_t LocalChallengeCode[LoLaCryptoDefinition::CHALLENGE_CODE_SIZE];
	uint8_t PartnerChallengeCode[LoLaCryptoDefinition::CHALLENGE_CODE_SIZE];
	uint8_t PartnerChallengeSignature[LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE];

	/// <summary>
	/// Shared secret key.
	/// </summary>
	uint8_t SecretKey[LoLaCryptoDefinition::CYPHER_KEY_SIZE];

private:
	const uint8_t* LocalPublicKey = nullptr;
	const uint8_t* LocalPrivateKey = nullptr;
	const uint8_t* AccessPassword = nullptr;

public:
	LoLaCryptoPkeSession()
		: LoLaCryptoSession()
		, ECC_CURVE(uECC_secp160r1())
	{
	}

	void SetKeysAndPassword(
		const uint8_t* publicKey,
		const uint8_t* privateKey,
		const uint8_t* accessPassword)
	{
		LocalPublicKey = publicKey;
		LocalPrivateKey = privateKey;
		AccessPassword = accessPassword;

		return;
	}

	const bool Setup()
	{
		return LocalPublicKey != nullptr && LocalPrivateKey != nullptr && AccessPassword != nullptr;
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

	void GetChallengeSignature(const uint8_t* challenge, const uint8_t* password, uint8_t* signatureTarget)
	{
		KeyHasher.reset(LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE);
		KeyHasher.update(challenge, LoLaCryptoDefinition::CHALLENGE_CODE_SIZE);
		KeyHasher.update(password, LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE);

		KeyHasher.finalize(signatureTarget, LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE);
	}

	const bool VerifyChallengeSignature(const uint8_t* signatureSource)
	{
		GetChallengeSignature(LocalChallengeCode, AccessPassword, PartnerChallengeSignature);

		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE; i++)
		{
			if (PartnerChallengeSignature[i] != signatureSource[i])
			{
				return false;
			}
		}

		return true;
	}

	void SetPartnerChallenge(const uint8_t* challengeSource)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::CHALLENGE_CODE_SIZE; i++)
		{
			PartnerChallengeCode[i] = challengeSource[i];
		}
	}

	void CopyLocalChallengeTo(uint8_t* challengeTarget)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::CHALLENGE_CODE_SIZE; i++)
		{
			challengeTarget[i] = LocalChallengeCode[i];
		}
	}

	void SignPartnerChallengeTo(uint8_t* signatureTarget)
	{
		GetChallengeSignature(PartnerChallengeCode, AccessPassword, signatureTarget);
	}

	void SignLocalChallengeTo(uint8_t* signatureTarget)
	{
		GetChallengeSignature(LocalChallengeCode, AccessPassword, signatureTarget);
	}

	void GenerateLocalChallenge(LoLaRandom* randomSource)
	{
		randomSource->GetRandomStreamCrypto(LocalChallengeCode, LoLaCryptoDefinition::CHALLENGE_CODE_SIZE);
	}
};
#endif