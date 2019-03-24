// LoLaCryptoTokenSource.h

#ifndef _LOLACRYPTOTOKENSOURCE_h
#define _LOLACRYPTOTOKENSOURCE_h

#include <Services\Link\LoLaLinkDefinitions.h>
#include <LoLaClock\ILoLaClockSource.h>
#include <LoLaCrypto\ITokenSource.h>


class LoLaCryptoTokenSource : public ITokenSource
{
private:
	uint32_t TOTPPeriodMillis = ILOLA_INVALID_MILLIS;
	uint32_t TOTPSeed = 0;

public:
	void SetTOTPPeriod(const uint32_t totpPeriodMillis)
	{
		TOTPPeriodMillis = totpPeriodMillis;
	}

	void SetSeed(const uint32_t seed)
	{
		TOTPSeed = seed;
	}

	uint32_t GetSeed()
	{
		return TOTPSeed;
	}

	uint32_t GetToken(const uint32_t syncMillis)
	{
		return (syncMillis / TOTPPeriodMillis) ^ TOTPSeed;
	}
};
#endif