// LoLaCryptoAmSession.h

#ifndef _LOLA_CRYPTO_AM_SESSION_h
#define _LOLA_CRYPTO_AM_SESSION_h

#include "LoLaCryptoEncoderSession.h"

/// <summary>
/// Public Address Exchange session based on magic.
/// </summary>
class LoLaCryptoAmSession final : public LoLaCryptoEncoderSession
{
private:
	enum class AmEnum
	{
		NoPartner,
		CalculatingLinkingToken,
		CalculatingMatch,
		CalculatingExpandedKey,
		CalculatingAddressing,
		AmCached
	};

private:
	uint8_t PartnerAddress[PUBLIC_ADDRESS_SIZE]{};

	/// <summary>
	/// Shared key.
	/// </summary>
	uint8_t MatchKey[SECRET_KEY_SIZE]{};

private:
	const uint8_t* SecretKey;
	const uint8_t* LocalAddress;

	AmEnum AmState = AmEnum::NoPartner;

public:
	/// <summary>
	/// </summary>
	/// <param name="secretKey">sizeof = LoLaLinkDefinition::SECRET_KEY_SIZE</param>
	/// <param name="accessPassword">sizeof = LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE</param>
	/// <param name="localAddress">sizeof = LoLaCryptoDefinition::PUBLIC_KEY_SIZE</param>
	LoLaCryptoAmSession(
		const uint8_t secretKey[SECRET_KEY_SIZE],
		const uint8_t accessPassword[ACCESS_CONTROL_PASSWORD_SIZE],
		const uint8_t localAddress[PUBLIC_KEY_SIZE])
		: LoLaCryptoEncoderSession(accessPassword)
		, SecretKey(secretKey)
		, LocalAddress(localAddress)
	{}

protected:
	const LoLaLinkDefinition::LinkType GetLinkType() final
	{
		return LoLaLinkDefinition::LinkType::AddressMatch;
	}

public:
	const bool Setup() final
	{
		return LoLaCryptoEncoderSession::Setup()
			&& SecretKey != nullptr
			&& LocalAddress != nullptr;
	}

public:
	const bool HasPartner()
	{
		return AmState != AmEnum::NoPartner;
	}

	const bool Ready()
	{
		return AmState == AmEnum::AmCached;
	}

	void ResetAm()
	{
		AmState = AmEnum::NoPartner;
	}

	void CopyLocalAddressTo(uint8_t* target)
	{
		for (uint_fast8_t i = 0; i < PUBLIC_ADDRESS_SIZE; i++)
		{
			target[i] = LocalAddress[i];
		}
	}

	void CopyPartnerAddressTo(uint8_t* target)
	{
		for (uint_fast8_t i = 0; i < PUBLIC_ADDRESS_SIZE; i++)
		{
			target[i] = PartnerAddress[i];
		}
	}

	void SetPartnerAddressFrom(const uint8_t* source)
	{
		for (uint_fast8_t i = 0; i < PUBLIC_ADDRESS_SIZE; i++)
		{
			PartnerAddress[i] = source[i];
		}

		AmState = AmEnum::CalculatingLinkingToken;
	}

	const bool LocalAddressMatchFrom(const uint8_t* source)
	{
		for (uint_fast8_t i = 0; i < PUBLIC_ADDRESS_SIZE; i++)
		{
			if (LocalAddress[i] != source[i])
			{
				return false;
			}
		}

		return true;
	}

	/// <summary>
	/// Long operations, cannot be done in line with linking protocol.
	/// </summary>
	/// <returns></returns>
	const bool Calculate()
	{
		switch (AmState)
		{
		case AmEnum::NoPartner:
			return false;
			break;
		case AmEnum::CalculatingLinkingToken:
			CalculateLinkingToken(LocalAddress, PartnerAddress, PUBLIC_ADDRESS_SIZE);
			AmState = AmEnum::CalculatingMatch;
			break;
		case AmEnum::CalculatingMatch:
			CalculateMatchKey();
			AmState = AmEnum::CalculatingExpandedKey;
			break;
		case AmEnum::CalculatingExpandedKey:
			CalculateExpandedKey(MatchKey, SECRET_KEY_SIZE);
			ClearMatchKey();
			AmState = AmEnum::CalculatingAddressing;
			break;
		case AmEnum::CalculatingAddressing:
			CalculateSessionAddressing(LocalAddress, PartnerAddress, PUBLIC_ADDRESS_SIZE);
			AmState = AmEnum::AmCached;
			break;
		case AmEnum::AmCached:
			return true;
		default:
			break;
		}

		return false;
	}

	const bool AddressCollision()
	{
		for (uint_fast8_t i = 0; i < PUBLIC_ADDRESS_SIZE; i++)
		{
			if (LocalAddress[i] != PartnerAddress[i])
			{
				return false;
			}
		}
		return true;
	}

private:
	void ClearMatchKey()
	{
		for (uint_fast8_t i = 0; i < SECRET_KEY_SIZE; i++)
		{
			MatchKey[i] = 0;
		}
	}

	void CalculateMatchKey()
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(SecretKey, SECRET_KEY_SIZE);
		CryptoHasher.update(LinkingToken, LINKING_TOKEN_SIZE);
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, MatchKey, SECRET_KEY_SIZE);
#else
		CryptoHasher.finalize(MatchKey, SECRET_KEY_SIZE);
#endif
		CryptoHasher.clear();
	}
};
#endif