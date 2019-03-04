// PseudoMacGenerator.h

#ifndef _PSEUDOMACGENERATOR_h
#define _PSEUDOMACGENERATOR_h

#include <stdint.h>
#include <LoLaCrypto\TinyCRC.h>
#include <LoLaCrypto\UniqueIdProvider.h>

template<const uint8_t MACLength>
class LoLaMAC
{
private:
	uint8_t MAC[MACLength];
	uint32_t MACHash = 0;
	bool Cached = false;

	TinyCrcModbus8 CRC;

	///Unique Id
	UniqueIdProvider IdProvider;
	///

private:
	void UpdateMAC()
	{
		CRC.Reset();

		for (uint8_t i = 0; i < (MACLength - 1); i++)
		{
			CRC.Update(IdProvider.GetUUIDPointer()[i]);
			MAC[i] = CRC.GetCurrent();
		}

		//Use remaining entropy of ID by hashing the last unused bytes
		// into the last byte of the MAC.
		if (MACLength < IdProvider.GetUUIDLength())
		{
			CRC.Reset();
			CRC.Update(&IdProvider.GetUUIDPointer()[MACLength - 1], IdProvider.GetUUIDLength() - MACLength - 1);
			MAC[MACLength - 1] = CRC.GetCurrent();
		}
		else
		{
			MAC[MACLength - 1] = IdProvider.GetUUIDPointer()[MACLength - 1];
		}

		UpdateMACHash();
	}

	inline void UpdateMACHash()
	{
		//TODO: Upgrade to real 32 bit hash
		CRC.Reset();
		CRC.Update(MAC, 0);
		MACHash = CRC.GetCurrent();
		CRC.Update(MAC, 1);
		MACHash += CRC.GetCurrent() << 8;
		CRC.Update(MAC, 2);
		MACHash += CRC.GetCurrent() << 16;
		CRC.Update(MAC, 3);
		MACHash += CRC.GetCurrent() << 24;		
	}

	inline void LazyLoad()
	{
		//Lazy loaded MAC.
		if (!Cached)
		{
			//It is not the MAC generator's job to get enough entropy.
			if (MACLength <= IdProvider.GetUUIDLength())
			{
				UpdateMAC();
				Cached = true;
			}			
		}
	}

public:
	bool Match(uint8_t * mac)
	{
		return Match((uint8_t*)&MAC, mac);
	}

	bool Match(uint8_t * mac1, uint8_t* mac2)
	{
		for (uint8_t i = 0; i < MACLength; i++)
		{
			if (mac1[i] != mac2[i])
			{
				return false;
			}
		}		

		return true;
	}

	uint32_t GetMACHash()
	{
		LazyLoad();

		return MACHash;
	}

	uint8_t* GetMACPointer()
	{
		LazyLoad();

		return MAC;
	}
};
#endif