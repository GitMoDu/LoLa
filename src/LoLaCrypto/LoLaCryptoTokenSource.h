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

	ILoLaClockSource* SyncedClock = nullptr;

public:
	void SetTOTPPeriod(const uint32_t totpPeriodMillis)
	{
		TOTPPeriodMillis = totpPeriodMillis;
	}

	bool Setup(ILoLaClockSource* syncedClock)
	{
		SyncedClock = syncedClock;

		return SyncedClock != nullptr;
	}

	void SetSeed(const uint32_t seed)
	{
		TOTPSeed = seed;
	}

	uint32_t GetSeed()
	{
		return TOTPSeed;
	}

	uint32_t GetToken()
	{
		return ((SyncedClock->GetSyncMicros() / (uint32_t)1000) / TOTPPeriodMillis) ^ TOTPSeed;
	}

	uint32_t GetNextSwitchOverMillis()
	{
		return TOTPPeriodMillis - ((SyncedClock->GetSyncMicros() / (uint32_t)1000) % TOTPPeriodMillis);
	}
};
#endif