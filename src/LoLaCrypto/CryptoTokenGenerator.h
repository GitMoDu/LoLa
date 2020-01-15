// CryptoTokenGenerator.h.h

#ifndef _LOLACRYPTOTOKENGENERATOR_h
#define _LOLACRYPTOTOKENGENERATOR_h

#include <LoLaClock\IClock.h>
#include <LoLaCrypto\ITokenSource.h>
#include <LoLaDefinitions.h>

class TimedHopProvider
{
public:
	bool TimedHopEnabled = false;
};

template<const uint32_t HopPeriodMillis,
	const uint8_t SeedSize>
	class TOTPMillisecondsTokenGenerator : public ITokenSource
{
protected:
	static const uint8_t TokenSize = LOLA_LINK_CRYPTO_TOKEN_SIZE;

	static const uint32_t ONE_MILLI_MICROS = 1000;

	static const uint32_t HopPeriodMicros = HopPeriodMillis * ONE_MILLI_MICROS;

	uint32_t Seed = 0;
	uint32_t CurrentToken = 0;

	ISyncedClock* SyncedClock = nullptr;

	TimedHopProvider* HopEnabled = nullptr;

protected:
	virtual uint32_t GetTimeStamp()
	{
		return 0;
	}

public:
	TOTPMillisecondsTokenGenerator()
	{

	}

	// Current version only supports uint32_t Seeds and Tokens.
	bool Setup(ISyncedClock* syncedClock, TimedHopProvider* hopEnabled)
	{
		SyncedClock = syncedClock;
		HopEnabled = hopEnabled;

		if (SyncedClock == nullptr &&
			HopEnabled == nullptr )
		{
			Serial.println(F("Pointers bitch"));

		}

		if (
			TokenSize != sizeof(uint32_t) &&
			SeedSize != sizeof(uint32_t))
		{
			Serial.println(F("Sizes bitch"));

		}

		return SyncedClock != nullptr &&
			HopEnabled != nullptr &&
			TokenSize == sizeof(uint32_t) &&
			SeedSize == sizeof(uint32_t);
	}

	void SetSeed(const uint32_t seed)
	{
		Seed = seed;
	}

	uint32_t GetToken()
	{
		if (HopEnabled->TimedHopEnabled)
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