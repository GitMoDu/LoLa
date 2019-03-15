// LoLaCryptoTokenSource.h

#ifndef _LOLACRYPTOTOKENSOURCE_h
#define _LOLACRYPTOTOKENSOURCE_h

#include <Services\Link\LoLaLinkDefinitions.h>
#include <ClockSource.h>
#include <LoLaCrypto\ITokenSource.h>


class LoLaCryptoTokenSource : public ITokenSource
{
private:
	uint32_t TOTPPeriod = ILOLA_INVALID_MILLIS;
	uint32_t TOTPSeed = 0;

	ClockSource* SyncedClock = nullptr;

	//Helper.
	uint32_t SwitchOverHelper = 0;

public:
	void SetTOTPPeriod(const uint32_t totpPeriodMillis)
	{
		TOTPPeriod = totpPeriodMillis;
	}

	bool Setup(ClockSource* syncedClock)
	{
		SyncedClock = syncedClock;

		return SyncedClock != nullptr;
	}

	void SetSeed(const uint32_t seed)
	{
		TOTPSeed = seed;
	}

	uint32_t GetToken()
	{
		return (SyncedClock->GetSyncMillis() / TOTPPeriod) ^ TOTPSeed;
	}

	uint32_t GetNextSwitchOverMillis()
	{
		SwitchOverHelper = SyncedClock->GetSyncMillis();

		return (((SwitchOverHelper + TOTPPeriod) / TOTPPeriod)*TOTPPeriod) - SwitchOverHelper;
	}
};
#endif