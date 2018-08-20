#ifndef _UNIQUE_ID_PROVIDER_h
#define _UNIQUE_ID_PROVIDER_h

#include <Arduino.h>


#define UNIQUE_ID_MAX_LENGTH	16


#if defined(ARDUINO_ARCH_STM32F1)
//For STM32, unique ID is read straight from the IC.
#define ID_ADDRESS_POINTER		(0x1FFFF7E8)
#define ID_STM32_LENGTH			12
#elif defined(ARDUINO_ARCH_AVR)
//For AVR Arduino: first UNIQUE_ID_MAX_LENGTH + 1 bytes of EEPROM are reserved for Serial Id.
#include <EEPROM.h>
#include <Crypto\TinyCRC.h>
#else
#error Platform not supported.
#endif

class UniqueIdProvider
{
private:
	uint8_t UUID[UNIQUE_ID_MAX_LENGTH];

public:
	UniqueIdProvider()
	{
		ClearUUID();

#if defined(ARDUINO_ARCH_STM32F1)
		uint16 *idBase0 = (uint16 *)ID_ADDRESS_POINTER;
		byte* bytes = (byte*)idBase0;
		for (uint8_t i = 0; i < ID_STM32_LENGTH; i++) {
			UUID[i] = bytes[i];
		}
		for (uint8_t i = ID_STM32_LENGTH; i < UNIQUE_ID_MAX_LENGTH; i++) {
			UUID[i] = 0;
		}

#elif defined(ARDUINO_ARCH_AVR)

		if (!ReadSerialFromEEPROM())
		{
			WriteNewSerialToEEPROM();
		}
#endif
	}

	void ClearUUID()
	{
		for (int i = 0; i < UNIQUE_ID_MAX_LENGTH; i++) {
			UUID[i] = 0;
		}
	}

	uint8_t GetUUIDLength() const
	{

		return UNIQUE_ID_MAX_LENGTH;
	}

	uint8_t * GetUUIDPointer()
	{

		return &UUID;
	}
	
#ifdef ARDUINO_ARCH_AVR
	bool ReadSerialFromEEPROM()
	{
		TinyCrcModbus8 CalculatorCRC;

		CalculatorCRC.Reset();
		for (uint8_t i = 0; i < UNIQUE_ID_MAX_LENGTH; i++) {
			UUID[i] = EEPROM.read(i);
			CalculatorCRC.Update(UUID[i]);
		}

		if (CalculatorCRC.GetCurrent() == EEPROM.read(UNIQUE_ID_MAX_LENGTH))
		{
			return true;
		}

		ClearUUID();

		return false;
	}


	void WriteNewSerialToEEPROM()
	{
		ExpandEntropy();

		for (uint8_t i = 0; i < UNIQUE_ID_MAX_LENGTH; i++) {
			UUID[i] = random(UINT8_MAX);
			EEPROM.write(i, UUID[i]);

			if (UUID[i] != EEPROM.read(i))//Make sure the data is there.
			{
				//ERROR, ERROR, unable to save id.
				ClearUUID();
				return;
			}
			CalculatorCRC.Update(UUID[i]);
		}
		
		EEPROM.write(UNIQUE_ID_MAX_LENGTH, CalculatorCRC.GetCurrent());

		if (CalculatorCRC.GetCurrent() != EEPROM.read(UNIQUE_ID_MAX_LENGTH))//Make sure the data is there.
		{
			//ERROR, ERROR, unable to save id.
			ClearUUID();
			return;
		}
	}

	void ExpandEntropy()
	{
		uint32_t WeakEntropySource = 0;

		WeakEntropySource += micros();
		WeakEntropySource += analogRead(A0);
		WeakEntropySource += analogRead(A1);
		WeakEntropySource += analogRead(A2);
		WeakEntropySource += analogRead(A3);
		WeakEntropySource += analogRead(A4);
		WeakEntropySource += analogRead(A5);

		randomSeed(WeakEntropySource);
	}
#endif

};
#endif
