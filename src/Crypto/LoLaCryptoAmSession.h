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
		CalculatingMatch,
		CalculatingExpandedKey,
		CalculatingAddressing,
		CalculatingLinkingToken,
		AmCached
	};

private:
	uint8_t PartnerAddress[LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE]{};

	/// <summary>
	/// Shared key.
	/// </summary>
	uint8_t MatchKey[LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE]{};

private:
	const uint8_t* LocalAddress = nullptr;

	AmEnum AmState = AmEnum::CalculatingMatch;


public:
	/// <summary>
	/// </summary>
	/// <param name="accessPassword">sizeof = LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE</param>
	/// <param name="localAddress">sizeof = LoLaCryptoDefinition::PUBLIC_KEY_SIZE</param>
	LoLaCryptoAmSession(const uint8_t* accessPassword, const uint8_t* localAddress)
		: LoLaCryptoEncoderSession(accessPassword)
		, LocalAddress(localAddress)
	{}

protected:
	virtual const LoLaLinkDefinition::LinkType GetLinkType() final
	{
		return LoLaLinkDefinition::LinkType::AddressMatch;
	}

public:
	virtual const bool Setup() final
	{
		return LoLaCryptoEncoderSession::Setup() &&
			LocalAddress != nullptr;
	}

public:
	const bool SessionIsCached()
	{
		return AmState == AmEnum::AmCached;
	}

	void ResetAm()
	{
		AmState = AmEnum::CalculatingMatch;
	}

	void CopyLocalAddressTo(uint8_t* target)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE; i++)
		{
			target[i] = LocalAddress[i];
		}
	}

	void CopyPartnerAddressTo(uint8_t* target)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE; i++)
		{
			target[i] = PartnerAddress[i];
		}
	}

	void SetPartnerAddressFrom(const uint8_t* source)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE; i++)
		{
			PartnerAddress[i] = source[i];
		}
	}

	const bool LocalAddressMatchFrom(const uint8_t* source)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE; i++)
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
		case AmEnum::CalculatingMatch:
			CalculateMatchKey();
			AmState = AmEnum::CalculatingExpandedKey;
			break;
		case AmEnum::CalculatingExpandedKey:
			CalculateExpandedKey(MatchKey, LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE);
			AmState = AmEnum::CalculatingAddressing;
			break;
		case AmEnum::CalculatingAddressing:
			CalculateSessionAddressing(
				LocalAddress,
				PartnerAddress,
				LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE);
			AmState = AmEnum::CalculatingLinkingToken;
			break;
		case AmEnum::CalculatingLinkingToken:
			CalculateLinkingToken();
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
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE; i++)
		{
			if (LocalAddress[i] != PartnerAddress[i])
			{
				return false;
			}
		}
		return true;
	}

private:
	void CalculateMatchKey()
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE; i++)
		{
			MatchKey[i] = LocalAddress[i] ^ PartnerAddress[i];
		}
	}
};
#endif