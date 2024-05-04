// LoLaCryptoPkeSession.h

#ifndef _LOLA_CRYPTO_PKE_SESSION_h
#define _LOLA_CRYPTO_PKE_SESSION_h

#include "LoLaCryptoEncoderSession.h"

/*
* https://github.com/kmackay/micro-ecc
*/
#include <uECC.h>

/// <summary>
/// Public Key Exchange session based on uECC_secp160r1.
/// </summary>
class LoLaCryptoPkeSession final : public LoLaCryptoEncoderSession
{
private:
	enum class PkeEnum
	{
		CalculatingSecret,
		CalculatingExpandedKey,
		CalculatingAddressing,
		CalculatingLinkingToken,
		CalculatingSessionToken,
		PkeCached
	};

	static constexpr uint8_t MATCHING_TOKEN_SIZE = 4;

private:
	const uECC_Curve_t* ECC_CURVE; // uECC_secp160r1

private:
	/// <summary>
	/// Ephemeral session matching token.
	/// Only valid if PkeState == PkeCached.
	/// </summary>
	uint8_t CachedToken[MATCHING_TOKEN_SIZE]{};

	PkeEnum PkeState = PkeEnum::CalculatingSecret;

private:
	uint8_t PartnerPublicKey[PUBLIC_KEY_SIZE]{};

	/// <summary>
	/// Shared secret key.
	/// </summary>
	uint8_t SecretKey[SHARED_KEY_SIZE]{};

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
	LoLaCryptoPkeSession(const uint8_t accessPassword[ACCESS_CONTROL_PASSWORD_SIZE],
		const uint8_t publicKey[PUBLIC_KEY_SIZE],
		const uint8_t privateKey[PRIVATE_KEY_SIZE])
		: LoLaCryptoEncoderSession(accessPassword)
		, ECC_CURVE(uECC_secp160r1())
		, LocalPublicKey(publicKey)
		, LocalPrivateKey(privateKey)
	{
		uECC_set_rng(0);
	}

public:
	const bool SessionIsCached()
	{
		return PkeState == PkeEnum::PkeCached && SessionTokenMatches(PartnerPublicKey);
	}

protected:
	virtual const LoLaLinkDefinition::LinkType GetLinkType() final
	{
		return LoLaLinkDefinition::LinkType::PublicKeyExchange;
	}

public:
	virtual const bool Setup() final
	{
		return LoLaCryptoEncoderSession::Setup() &&
			LocalPublicKey != nullptr
			&& LocalPrivateKey != nullptr;
	}

	virtual void SetSessionId(const uint8_t* sessionId)
	{
		LoLaCryptoEncoderSession::SetSessionId(sessionId);
		for (uint_fast8_t i = 0; i < MATCHING_TOKEN_SIZE; i++)
		{
			CachedToken[i] = 0;
		}
	}

	virtual void SetRandomSessionId(LoLaRandom* randomSource) final
	{
		LoLaCryptoEncoderSession::SetRandomSessionId(randomSource);
		for (uint_fast8_t i = 0; i < MATCHING_TOKEN_SIZE; i++)
		{
			CachedToken[i] = 0;
		}
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
			PkeState = PkeEnum::CalculatingExpandedKey;
			break;
		case PkeEnum::CalculatingExpandedKey:
			CalculateExpandedKey(SecretKey, SHARED_KEY_SIZE);
			PkeState = PkeEnum::CalculatingAddressing;
			break;
		case PkeEnum::CalculatingAddressing:
			CalculateSessionAddressing(
				LocalPublicKey,
				PartnerPublicKey,
				PUBLIC_KEY_SIZE);
			PkeState = PkeEnum::CalculatingLinkingToken;
			break;
		case PkeEnum::CalculatingLinkingToken:
			CalculateLinkingToken(LocalPublicKey,
				PartnerPublicKey,
				PUBLIC_KEY_SIZE);
			PkeState = PkeEnum::CalculatingSessionToken;
			break;
		case PkeEnum::CalculatingSessionToken:
			CalculateSessionToken(PartnerPublicKey, CachedToken);
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
		for (uint_fast8_t i = 0; i < PUBLIC_KEY_SIZE; i++)
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

private:
	/// <summary>
	/// Generates a unique id for this session id and partner.
	/// Assumes SessionId has been set.
	/// </summary>
	/// <param name="partnerPublicKey"></param>
	/// <param name="token"></param>
	void CalculateSessionToken(const uint8_t* partnerPublicKey, uint8_t* token)
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(SessionId, SESSION_ID_SIZE);
		CryptoHasher.update(partnerPublicKey, PUBLIC_KEY_SIZE);
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, token, MATCHING_TOKEN_SIZE);
#else
		CryptoHasher.finalize(token, MATCHING_TOKEN_SIZE);
#endif
		CryptoHasher.clear();
	}

	/// <summary>
	/// Assumes SessionId has been set.
	/// </summary>
	/// <param name="partnerPublicKey"></param>
	/// <returns>True if the calculated secrets are already set for this SessionId and PublicKey</returns>
	const bool SessionTokenMatches(const uint8_t* partnerPublicKey)
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(SessionId, SESSION_ID_SIZE);
		CryptoHasher.update(partnerPublicKey, PUBLIC_KEY_SIZE);

#if defined(LOLA_USE_POLY1305)
		const bool match = CryptoHasher.macMatches(Nonce, CachedToken);
#else
		const bool match = CryptoHasher.macMatches(CachedToken);
#endif
		CryptoHasher.clear();

		return match;
	}
};
#endif