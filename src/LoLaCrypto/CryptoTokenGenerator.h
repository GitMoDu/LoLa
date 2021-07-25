// CryptoTokenGenerator.h.h

#ifndef _LOLACRYPTOTOKENGENERATOR_h
#define _LOLACRYPTOTOKENGENERATOR_h

#include <LoLaClock\LoLaClock.h>
#include <LoLaCrypto\ITokenSource.h>
#include <LoLaDefinitions.h>

class AbstractCryptoTokenGenerator
{
public:
	const uint32_t HopPeriod;

protected:
	uint32_t Seed = 0;
	ISyncedClock* SyncedClock;

public:
	AbstractCryptoTokenGenerator(ISyncedClock* syncedClock, const uint32_t hopPeriod)
		: SyncedClock(syncedClock)
		, HopPeriod(hopPeriod)
	{
	}

	const uint32_t GetSeed()
	{
		return Seed;
	}

	void SetSeed(const uint32_t seed)
	{
		Seed = seed;
	}
};


// If changed, update ProtocolVersionCalculator.
class TOTPMillisecondsTokenGenerator : public AbstractCryptoTokenGenerator
{
public:
	TOTPMillisecondsTokenGenerator(ISyncedClock* syncedClock, const uint32_t hopPeriodMillis)
		: AbstractCryptoTokenGenerator(syncedClock, hopPeriodMillis)
	{
	}

	const uint32_t GetToken()
	{
		const uint32_t HopIndex = SyncedClock->GetSyncMillis() / HopPeriod;

		return Seed ^ HopIndex;
	}

	const uint32_t GetToken(const int8_t offsetMillis)
	{
		const uint32_t HopIndex = (SyncedClock->GetSyncMillis() + offsetMillis) / HopPeriod;

		// If changed, update ProtocolVersionCalculator.
		return Seed ^ HopIndex;
	}
};

// Fixed one second period token generator.
// If changed, update ProtocolVersionCalculator.
class TOTPSecondsTokenGenerator : public AbstractCryptoTokenGenerator
{
public:
	TOTPSecondsTokenGenerator(ISyncedClock* syncedClock)
		: AbstractCryptoTokenGenerator(syncedClock, IClock::ONE_SECOND)
	{}

	// Timing aware token getter.
	const uint32_t GetTokenFromEarlier(const uint8_t backwardsMillis)
	{
		uint32_t HopIndex;
		uint16_t Millis;
		SyncedClock->GetSyncSecondsMillis(HopIndex, Millis);

		if (((int16_t)Millis - backwardsMillis) < 0)
		{
			HopIndex--;
		}

		return Seed ^ HopIndex;
	}

	// Timing aware token getter.
	const uint32_t GetTokenFromAhead(const uint8_t forwardsMillis)
	{
		uint32_t HopIndex;
		uint16_t Millis;
		SyncedClock->GetSyncSecondsMillis(HopIndex, Millis);

		if ((Millis + forwardsMillis) > IClock::ONE_SECOND_MILLIS)
		{
			HopIndex++;
		}


		return Seed ^ HopIndex;
	}

	/*const uint32_t GetToken()
	{
		const uint32_t HopIndex = SyncedClock->GetSyncSeconds();

		return Seed ^ HopIndex;
	}*/
};
#endif