// LoLaCryptoTokenSource.h

#ifndef _LOLACRYPTOTOKENSOURCE_h
#define _LOLACRYPTOTOKENSOURCE_h

#include <Services\Link\LoLaLinkDefinitions.h>
#include <LoLaClock\ILoLaClockSource.h>
#include <LoLaCrypto\ITokenSource.h>

//
//class LoLaCryptoTokenSource : public ITokenSource
//{
//private:
//	uint32_t TOTPPeriodMillis = ILOLA_INVALID_MILLIS;
//	uint32_t TOTPSeed = 0;
//
//public:
//	uint32_t GetTOTPPeriod() { return TOTPPeriodMillis; }
//
//	void SetTOTPPeriod(const uint32_t totpPeriodMillis)
//	{
//		TOTPPeriodMillis = totpPeriodMillis;
//	}
//
//	void SetSeed(const uint32_t seed)
//	{
//		TOTPSeed = seed;
//	}
//
//	uint32_t GetSeed()
//	{
//		return TOTPSeed;
//	}
//
//	uint32_t GetToken(const uint32_t syncMillis)
//	{
//		return (syncMillis / TOTPPeriodMillis) ^ TOTPSeed;
//	}
//};

class LoLaTimedCryptoTokenSource : public ITokenSource
{
private:
	uint32_t Seed = 0;

	bool Enabled = false;

	//Synced clock source.
	ILoLaClockSource* SyncedClock = nullptr;

public:
	const static uint32_t PeriodMillis = LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS;

	uint32_t GetTOTPPeriod() { return PeriodMillis; }

public:
	LoLaTimedCryptoTokenSource(ILoLaClockSource* syncedClock)
	{
		SyncedClock = syncedClock;
	}

	bool Setup()
	{
		return SyncedClock != nullptr;
	}

	void SetSeed(const uint32_t seed)
	{
		Seed = seed;
	}

	uint32_t GetSeed()
	{
		return Seed;
	}

	void SetTOTPEnabled(const bool enabled)
	{
		Enabled = enabled;
	}

	inline uint32_t GetSyncMillis()
	{
		return SyncedClock->GetSyncMicros() / (uint32_t)1000;
	}

	uint32_t GetToken()
	{
		if (Enabled)
		{
			return (GetSyncMillis() / PeriodMillis) ^ Seed;
		}
		else
		{
			return Seed;
		}
	}
};
#endif