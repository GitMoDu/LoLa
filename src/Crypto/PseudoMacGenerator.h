// PseudoMacGenerator.h

#ifndef _PSEUDOMACGENERATOR_h
#define _PSEUDOMACGENERATOR_h

#include <stdint.h>
#include <Crypto\TinyCRC.h>
#include <Crypto\UniqueIdProvider.h>

template<const uint8_t MACLength>
class LoLaMAC
{
private:
	uint8_t MAC[MACLength];
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

		//Add extra entropy to MAC by hashing the last unused bytes
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
				Serial.println("No match");
				return false;
			}
		}		

		return true;
	}

	uint8_t* GetMACPointer()
	{
		//Lazy loaded MAC.
		if (!Cached)
		{
			//It is no the MAC generator's job to get enough entropy.
			if (MACLength > IdProvider.GetUUIDLength())
			{
				return nullptr;
			}

			UpdateMAC();
			Cached = true;
		}

		return MAC;
	}

#ifdef DEBUG_LOLA
	void Print(Stream* serial)
	{
		Print(serial, GetMACPointer());
	}

	void Print(Stream* serial, uint8_t* mac)
	{
		for (uint8_t i = 0; i < MACLength; i++)
		{
			serial->print(mac[i], HEX);
			if (i < MACLength - 1)
			{
				serial->print(':');
			}
		}
	}
#endif // DEBUG_LOLA

};
#endif