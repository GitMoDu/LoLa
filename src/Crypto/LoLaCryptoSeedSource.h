// LoLaCryptoSeedSource.h

#ifndef _LOLACRYPTOSEEDSOURCE_h
#define _LOLACRYPTOSEEDSOURCE_h

#include <Crypto\TinyCRC.h>

class LoLaCryptoSeedSource : public ISeedSource
{
private:
	uint8_t CachedBaseCRC = 0;
	TinyCrcModbus8 CalculatorCRC;
	boolean TOTPEnabled = false;

	uint8_t Offset = 0;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} PMACToArray;

private:
	uint32_t GetTOTP()
	{
		//TODO: Get extra TOTP seed
		return 0;
	}

public:
	void Reset()
	{
		CalculatorCRC.Reset();
		CachedBaseCRC = 0;
		TOTPEnabled = false;
	}

	void SetTOTPEnabled(const bool enabled)
	{
		TOTPEnabled = enabled;
	}

	inline void PMACToArrayToCRC()
	{
		CalculatorCRC.Update(PMACToArray.array[0]);
		CalculatorCRC.Update(PMACToArray.array[1]);
		CalculatorCRC.Update(PMACToArray.array[2]);
		CalculatorCRC.Update(PMACToArray.array[3]);
	}

	void SetBaseSeed(const uint32_t hostPMAC, const uint32_t remotePMAC, const uint8_t sessionId)
	{
		CalculatorCRC.Reset();

		PMACToArray.uint = hostPMAC;
		PMACToArrayToCRC();

		PMACToArray.uint = remotePMAC;
		PMACToArrayToCRC();

		CalculatorCRC.Update(sessionId);

		CachedBaseCRC = CalculatorCRC.GetCurrent();

		if (CachedBaseCRC == 0)
		{
			CachedBaseCRC++;
		}
	}

	uint8_t GetSeed()
	{
		if (false && TOTPEnabled)//TODO:
		{
			CalculatorCRC.Reset(CachedBaseCRC);

			PMACToArray.uint = GetTOTP();
			PMACToArrayToCRC();

			return CalculatorCRC.GetCurrent();
		}
		else
		{
			return CachedBaseCRC;
		}		
	}
};
#endif