// CryptoTokenGenerator.h.h

#ifndef _LOLACRYPTOTOKENGENERATOR_h
#define _LOLACRYPTOTOKENGENERATOR_h

#include <LoLaClock\IClock.h>
#include <LoLaCrypto\ITokenSource.h>
#include <LoLaDefinitions.h>


class TOTPMillisecondsTokenGenerator : public ITokenSource
{
protected:
	static const uint8_t TokenSize = LOLA_LINK_CRYPTO_TOKEN_SIZE;
	static const uint8_t SeedSize = LOLA_LINK_CRYPTO_SEED_SIZE;

	static const uint8_t HopPeriodMillis = LOLA_LINK_SERVICE_LINKED_TOKEN_HOP_PERIOD_MILLIS;
	static const uint32_t ONE_MILLI_MICROS = 1000;

	static const uint32_t HopPeriodMicros = HopPeriodMillis * ONE_MILLI_MICROS;

	uint32_t Seed = 0;
	uint32_t CurrentToken = 0;

	ISyncedClock* SyncedClock = nullptr;

	bool* TOTPEnabled = nullptr;

protected:
	virtual uint32_t GetTimeStamp()
	{
		return 0;
	}

public:
	TOTPMillisecondsTokenGenerator(ISyncedClock* syncedClock, bool* totpEnabled = nullptr)
	{
		SyncedClock = syncedClock;
		TOTPEnabled = totpEnabled;
	}

	bool Setup()
	{
		return SyncedClock != nullptr &&
			TOTPEnabled != nullptr &&
			TokenSize == sizeof(uint32_t) &&
			SeedSize == sizeof(uint32_t);
	}

	void SetSeed(const uint32_t seed)
	{
		Seed = seed;
	}

	uint32_t GetToken()
	{
		if (&TOTPEnabled)
		{
			// Using a uint32_t token, we get a rollover period of ~70 minutes.
			// CurrentTimestamp = (SyncedClock->GetSyncMicros() / HopPeriodMicros) % UINT32_MAX;

			// Token = Seed ^ CurrentTimestamp
			CurrentToken = Seed ^ ((SyncedClock->GetSyncMicros() / HopPeriodMicros) % UINT32_MAX);
		}
		else
		{
			CurrentToken = 0;
		}

		return CurrentToken;
	}
};
#endif