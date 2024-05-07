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
	uint8_t PartnerAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE]{};

	/// <summary>
	/// Shared key.
	/// </summary>
	uint8_t MatchKey[LoLaLinkDefinition::SECRET_KEY_SIZE]{};

private:
	const uint8_t* SecretKey;
	const uint8_t* LocalAddress;

	AmEnum AmState = AmEnum::NoPartner;

public:
	LoLaCryptoAmSession() : LoLaCryptoEncoderSession()
	{}

public:
	const bool SetKeys(const uint8_t localAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE],
		const uint8_t secretKey[LoLaLinkDefinition::SECRET_KEY_SIZE],
		const uint8_t accessPassword[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE])
	{
		if (LoLaCryptoEncoderSession::SetAccessPassword(accessPassword)
			&& secretKey != nullptr
			&& localAddress != nullptr)
		{
			SecretKey = secretKey;
			LocalAddress = localAddress;

			return true;
		}

		return false;
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
		if (LocalAddress == nullptr)
		{
			return;
		}

		memcpy(target, LocalAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
	}

	void CopyPartnerAddressTo(uint8_t* target)
	{
		memcpy(target, PartnerAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
	}

	void SetPartnerAddressFrom(const uint8_t* source)
	{
		if (SecretKey != nullptr
			&& LocalAddress != nullptr)
		{
			memcpy(PartnerAddress, source, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
			AmState = AmEnum::CalculatingLinkingToken;
		}
	}

	const bool LocalAddressMatchFrom(const uint8_t* source)
	{
		if (LocalAddress == nullptr)
		{
			return false;
		}

		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE; i++)
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
			CalculateLinkingToken(LocalAddress, PartnerAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
			AmState = AmEnum::CalculatingMatch;
			break;
		case AmEnum::CalculatingMatch:
			CalculateMatchKey();
			AmState = AmEnum::CalculatingExpandedKey;
			break;
		case AmEnum::CalculatingExpandedKey:
			CalculateExpandedKey(MatchKey, LoLaLinkDefinition::SECRET_KEY_SIZE);
			ClearMatchKey();
			AmState = AmEnum::CalculatingAddressing;
			break;
		case AmEnum::CalculatingAddressing:
			CalculateSessionAddressing(LocalAddress, PartnerAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
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
		if (LocalAddress == nullptr)
		{
			return true;
		}
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE; i++)
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
		memset(MatchKey, 0, LoLaLinkDefinition::SECRET_KEY_SIZE);
	}

	void CalculateMatchKey()
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(SecretKey, LoLaLinkDefinition::SECRET_KEY_SIZE);
		CryptoHasher.update(LinkingToken, LoLaLinkDefinition::LINKING_TOKEN_SIZE);
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, MatchKey, SECRET_KEY_SIZE);
#else
		CryptoHasher.finalize(MatchKey, LoLaLinkDefinition::SECRET_KEY_SIZE);
#endif
		CryptoHasher.clear();
	}
};
#endif