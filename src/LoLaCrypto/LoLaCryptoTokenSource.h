// LoLaCryptoTokenSource.h

#ifndef _LOLACRYPTOTOKENSOURCE_h
#define _LOLACRYPTOTOKENSOURCE_h

#include <Services\Link\LoLaLinkDefinitions.h>
#include <ClockSource.h>
#include <LoLaCrypto\ITokenSource.h>

#include <Crypto.h>
#include <SHA256.h>
#include <string.h>

class LoLaCryptoTokenSource : public ITokenSource
{
private:
	uint32_t TOTPPeriod = ILOLA_INVALID_MILLIS;
	uint32_t TOTPSeed = 0;

	SHA256 Hasher;

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

	union ArrayToUint32 {
		byte array[sizeof(uint32_t)];
		uint32_t uint;
	} ATUI;

	uint32_t GetToken()
	{
		ATUI.uint = (SyncedClock->GetSyncMillis() / TOTPPeriod) ^ TOTPSeed;
		Hasher.reset();
		Hasher.update(ATUI.array, sizeof(uint32_t));
		Hasher.finalize(ATUI.array, sizeof(uint32_t));

		return ATUI.uint;
	}

	uint32_t GetNextSwitchOverMillis()
	{
		SwitchOverHelper = SyncedClock->GetSyncMillis();

		return (((SwitchOverHelper + TOTPPeriod) / TOTPPeriod)*TOTPPeriod) - SwitchOverHelper;
	}
};
#endif