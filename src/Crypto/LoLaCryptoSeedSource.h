// LoLaCryptoSeedSource.h

#ifndef _LOLACRYPTOSEEDSOURCE_h
#define _LOLACRYPTOSEEDSOURCE_h

#include <Crypto\TinyCRC.h>

// Hop period should be a multiple of Send Slot Period.
#define LOLA_CRYPTO_TOTP_PERIOD_MILLIS	(10*ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS) //Max 256 ms of hop period.


class LoLaCryptoSeedSource : public ISeedSource
{
private:
	uint8_t CachedToken = 0;
	TinyCrcModbus8 CalculatorCRC;
	boolean TOTPEnabled = false;

	const uint8_t TOTPPeriod = LOLA_CRYPTO_TOTP_PERIOD_MILLIS;
	uint32_t TOTPSeed = 0;

	uint8_t Offset = 0;

	ClockSource* SyncedClock = nullptr;

	//Helper.
	uint32_t TOTPIndex = 0;

private:
	uint32_t GetTOTP()
	{
		TOTPIndex = SyncedClock->GetMillis() / TOTPPeriod;

		return TOTPIndex;
	}

	uint8_t GetSeed()
	{
		if (TOTPSeed == 0)
		{
			return TOTPSeed + 1;
		}

		return TOTPSeed;
	}

public:
	void Reset()
	{
		CalculatorCRC.Reset();
		CachedToken = 0;
		TOTPEnabled = false;
	}

	void SetTOTPEnabled(const bool enabled, ClockSource* syncedClock, const uint32_t seed = 0)
	{
		SyncedClock = syncedClock;
		TOTPEnabled = enabled && (SyncedClock != nullptr);
		TOTPSeed = seed;
	}

	void SetBaseSeed(const uint32_t hostPMAC, const uint32_t remotePMAC, const uint8_t sessionId)
	{
		CalculatorCRC.Reset();

		CalculatorCRC.Update32(hostPMAC);
		CalculatorCRC.Update32(remotePMAC);
		CalculatorCRC.Update(sessionId);

		CachedToken = CalculatorCRC.GetCurrent();

		if (CachedToken == 0)
		{
			CachedToken++;
		}
	}

	uint8_t GetToken()
	{
		if (TOTPEnabled)
		{
			CalculatorCRC.Reset(CachedToken);

			CalculatorCRC.Update32(GetTOTP());

			return CalculatorCRC.GetCurrent();
		}
		else
		{
			return CachedToken;
		}
	}
};
#endif