// LoLaCryptoTokenSource.h

#ifndef _LOLACRYPTOTOKENSOURCE_h
#define _LOLACRYPTOTOKENSOURCE_h

#include <Crypto\TinyCRC.h>
#include <ILoLa.h>

class LoLaCryptoTokenSource : public ISeedSource
{
private:
	uint8_t CachedSeed = 0;
	TinyCrcModbus8 CalculatorCRC;
	boolean TOTPEnabled = false;

	uint32_t TOTPPeriod = ILOLA_CRYPTO_TOTP_PERIOD_MILLIS;
	uint32_t TOTPSeed = 0;

	uint8_t Offset = 0;

	ClockSource* SyncedClock = nullptr;

	//Helpers.
	uint32_t TOTPIndex = 0;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} TOTPCachedValue;

private:
	inline uint32_t GetTOTP(const int8_t offsetMillis)
	{
		TOTPIndex = SyncedClock->GetMillis() + offsetMillis;
		TOTPIndex ^= TOTPSeed;

		TOTPIndex /= TOTPPeriod;

		return TOTPIndex;
	}

public:
	void Reset()
	{
		CalculatorCRC.Reset();
		CachedSeed = 0;
		TOTPEnabled = false;
	}

	void SetTOTPPeriod(const uint32_t totpPeriodMillis)
	{
		TOTPPeriod = totpPeriodMillis;
	}

	void SetTOTPEnabled(const bool enabled, ClockSource* syncedClock, const uint32_t seed = 0)
	{
		SyncedClock = syncedClock;
		TOTPEnabled = enabled && (SyncedClock != nullptr);
		TOTPSeed = seed;
	}

	void SetBaseSeed(uint8_t * hostPMAC, uint8_t* remotePMAC, const uint8_t macLength, const uint8_t sessionId)
	{
		CalculatorCRC.Reset();

		CalculatorCRC.Update(hostPMAC, macLength);
		CalculatorCRC.Update(remotePMAC, macLength);
		CalculatorCRC.Update(sessionId);

		CachedSeed = CalculatorCRC.GetCurrent();

		if (CachedSeed == 0)
		{
			CachedSeed++;
		}
	}

	uint8_t GetToken(const int8_t offsetMillis)
	{
		if (TOTPEnabled)
		{
			TOTPCachedValue.uint = GetTOTP(offsetMillis);
			CalculatorCRC.Reset(CachedSeed);

			CalculatorCRC.Update(TOTPCachedValue.array, 4);

			return CalculatorCRC.GetCurrent();
		}
		else
		{
			return CachedSeed;
		}
	}

	uint32_t GetCurrentSeed() { return CachedSeed; }
};
#endif