// LoLaCryptoAmSession.h

#ifndef _LOLA_CRYPTO_AM_SESSION_h
#define _LOLA_CRYPTO_AM_SESSION_h

#include "LoLaCryptoEncoderSession.h"

/// <summary>
/// Address Match (AM) session based on addresses, access password and secret key.
/// </summary>
class LoLaCryptoAmSession final : public LoLaCryptoEncoderSession
{
private:
	enum class AmEnum
	{
		NoPartner,
		CalculatingExpandedKey,
		CalculatingAddressing,
		AmCached
	};

private:
	AmEnum AmState = AmEnum::NoPartner;

public:
	LoLaCryptoAmSession() : LoLaCryptoEncoderSession()
	{}

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

	void SetPartnerAddressFrom(const uint8_t* source)
	{
		if (LocalAddress == nullptr
			|| SecretKey == nullptr
			|| AccessPassword == nullptr)
		{
			return;
		}

		SetPartnerAddress(source);
		AmState = AmEnum::CalculatingExpandedKey;
	}

	/// <summary>
	/// Long operations, cannot be done in line with linking protocol.
	/// </summary>
	void Calculate()
	{
		if (LocalAddress == nullptr
			|| SecretKey == nullptr
			|| AccessPassword == nullptr)
		{
			return;
		}

		switch (AmState)
		{
		case AmEnum::NoPartner:
			break;
		case AmEnum::CalculatingExpandedKey:
			CalculateExpandedKey();
			AmState = AmEnum::CalculatingAddressing;
			break;
		case AmEnum::CalculatingAddressing:
			CalculateSessionAddressing();
			AmState = AmEnum::AmCached;
			break;
		case AmEnum::AmCached:
		default:
			break;
		}
	}
};
#endif