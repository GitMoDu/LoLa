// PseudoMacGenerator.h

#ifndef _PSEUDOMACGENERATOR_h
#define _PSEUDOMACGENERATOR_h

#include <stdint.h>
#include <LoLaCrypto\UniqueIdProvider.h>
#include <FastCRC.h>

template<const uint8_t MACLength>
class LoLaMAC
{
private:
	uint8_t MAC[MACLength];
	uint32_t MACHash = 0;
	bool Cached = false;

	FastCRC8 CRC8;

	///Unique Id
	UniqueIdProvider IdProvider;
	///

private:
	void UpdateMAC()
	{
		for (uint8_t i = 0; i < (MACLength - 1); i++)
		{
			if (i == 0)
			{
				MAC[i] = CRC8.smbus(&IdProvider.GetUUIDPointer()[i], sizeof(uint8_t));
			}
			else
			{
				MAC[i] = CRC8.smbus_upd(&IdProvider.GetUUIDPointer()[i], sizeof(uint8_t));
			}
		}

		//Use remaining entropy of ID by hashing the last unused bytes
		// into the last byte of the MAC.
		if (MACLength < IdProvider.GetUUIDLength())
		{
			MAC[MACLength - 1] = CRC8.smbus(&IdProvider.GetUUIDPointer()[MACLength - 1], IdProvider.GetUUIDLength() - MACLength - 1);
		}
		else
		{
			MAC[MACLength - 1] = IdProvider.GetUUIDPointer()[MACLength - 1];
		}

		UpdateMACHash();
	}

	inline void UpdateMACHash()
	{
		MACHash = CRC8.smbus(&MAC[0], sizeof(uint8_t));
		MACHash += CRC8.smbus_upd(&MAC[1], sizeof(uint8_t)) << 8;
		MACHash += CRC8.smbus_upd(&MAC[2], sizeof(uint8_t)) << 16;
		MACHash += CRC8.smbus_upd(&MAC[3], sizeof(uint8_t)) << 24;
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