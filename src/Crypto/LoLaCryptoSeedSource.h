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
		Offset = 0;
		TOTPEnabled = false;
	}

	void SetTOTPEnabled(const bool enabled)
	{
		//TOTPEnabled = enabled;//TODO:
	}

	void SetBaseSeed(const uint32_t hostPMAC, const uint32_t remotePMAC, const uint8_t sessionId)
	{
		CalculatorCRC.Reset();

		PMACToArray.uint = hostPMAC;
		for (uint8_t i = 0; i < sizeof(uint32_t); i++)
		{
			CalculatorCRC.Update(PMACToArray.array[i]);
		}

		PMACToArray.uint = remotePMAC;
		for (uint8_t i = 0; i < sizeof(uint32_t); i++)
		{
			CalculatorCRC.Update(PMACToArray.array[i]);
		}

		CalculatorCRC.Update(sessionId);

		CachedBaseCRC = CalculatorCRC.GetCurrent();

		if (CachedBaseCRC == 0)
		{
			CachedBaseCRC++;
		}		
	}

	uint8_t GetSeed()
	{
		if (TOTPEnabled)
		{
			CalculatorCRC.Reset(CachedBaseCRC);

			PMACToArray.uint = GetTOTP();
			for (uint8_t i = 0; i < sizeof(uint32_t); i++)
			{
				CalculatorCRC.Update(PMACToArray.array[i]);
			}

			return CalculatorCRC.GetCurrent();
		}
		else
		{
			return CachedBaseCRC;
		}		
	}
};
#endif