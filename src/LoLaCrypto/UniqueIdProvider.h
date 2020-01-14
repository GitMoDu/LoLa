#ifndef _UNIQUE_ID_PROVIDER_h
#define _UNIQUE_ID_PROVIDER_h

#include <Arduino.h>


#define UNIQUE_ID_MAX_LENGTH	16

#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
//For STM32, unique ID is read straight from the IC.
#define ID_ADDRESS_POINTER		(0x1FFFF7E8)
#define ID_STM32_LENGTH			12
#elif defined(ARDUINO_ARCH_AVR)
//For AVR Arduino: first UNIQUE_ID_MAX_LENGTH + 1 bytes of EEPROM are reserved for Serial Id.
#include <EEPROM.h>
#include <FastCRC.h>
#else
#error Platform not supported.
#endif

class UniqueIdProvider
{
private:
	uint8_t UUID[UNIQUE_ID_MAX_LENGTH];
	bool HasUUID = false;

#if defined(ARDUINO_ARCH_AVR)
	FastCRC8 CRC8;
#endif

private:
	void UpdateId()
	{
#if defined(ARDUINO_ARCH_STM32F1)
		uint16 *idBase0 = (uint16 *)ID_ADDRESS_POINTER;
		byte* bytes = (byte*)idBase0;
		for (uint8_t i = 0; i < min(ID_STM32_LENGTH, UNIQUE_ID_MAX_LENGTH); i++)
		{
			UUID[i] = bytes[i];
		}

		for (uint8_t i = min(ID_STM32_LENGTH, UNIQUE_ID_MAX_LENGTH); i < UNIQUE_ID_MAX_LENGTH; i++)
		{
			UUID[i] = 0;
		}

#elif defined(ARDUINO_ARCH_AVR)
		if (!ReadSerialFromEEPROM())
		{
			for (uint8_t i = 0; i < UNIQUE_ID_MAX_LENGTH; i++)
			{
				UUID[i] = random(UINT8_MAX);
			}
			WriteNewSerialToEEPROM();
		}
#endif
	}

public:
	uint8_t GetUUIDLength() const
	{
		return UNIQUE_ID_MAX_LENGTH;
	}

	uint8_t* GetUUIDPointer()
	{
		//Lazy loaded Id.
		if (!HasUUID)
		{
			UpdateId();
			HasUUID = true;
		}

		return UUID;
	}
	
#ifdef ARDUINO_ARCH_AVR
	bool ReadSerialFromEEPROM()
	{		
		for (uint8_t i = 0; i < UNIQUE_ID_MAX_LENGTH; i++) {
			UUID[i] = EEPROM.read(i);
		}

		if (EEPROM.read(UNIQUE_ID_MAX_LENGTH) == CRC8.smbus(UUID, UNIQUE_ID_MAX_LENGTH))
		{
			return true;
		}

		HasUUID = false;

		return false;
	}


	void WriteNewSerialToEEPROM()
	{
		for (uint8_t i = 0; i < UNIQUE_ID_MAX_LENGTH; i++) {
			UUID[i] = random(UINT8_MAX);
			EEPROM.write(i, UUID[i]);

			if (UUID[i] != EEPROM.read(i))//Make sure the data is there.
			{
				//ERROR, ERROR, unable to save id.
				HasUUID = false;
				return;
			}
		}

		uint8_t LastCrc = CRC8.smbus(UUID, UNIQUE_ID_MAX_LENGTH);
		EEPROM.write(UNIQUE_ID_MAX_LENGTH, LastCrc);

		if (LastCrc != EEPROM.read(UNIQUE_ID_MAX_LENGTH))//Make sure the data is there.
		{
			//ERROR, ERROR, unable to save id.
			HasUUID = false;
			return;
		}
	}
#endif
};
#endif