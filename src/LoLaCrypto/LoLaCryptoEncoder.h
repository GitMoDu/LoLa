// LoLaCryptoEncoder.h

#ifndef _LOLACRYPTOENCODER_h
#define _LOLACRYPTOENCODER_h

#include <Crypto.h>
#include <CryptoLW.h>
#include <Ascon128.h>
#include <Acorn128.h>
#include "utility/ProgMemUtil.h"

#include <LoLaCrypto\TinyCRC.h>

#include <Crypto.h>
#include <SHA256.h>

class LoLaCryptoEncoder
{
private:
	enum  StageEnum : uint8_t
	{
		AllClear,
		AllReady
	} EncoderState = StageEnum::AllClear;

	uint8_t KeyHolder[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	uint8_t IVHolder[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	uint8_t TokenHolder[4] = { 0, 0, 0, 0 };

	uint32_t TokenSeed = 0; //Last 4 bytes of key are used for token.

	bool CryptoEnable = false;


	///CRC validation.
	TinyCrcModbus8 CalculatorCRC;
	///


private:
	Ascon128 Cypher;
	SHA256 Hasher;

	union ArrayToUint32 {
		byte array[sizeof(uint32_t)];
		uint32_t uint;
	} ATUI;

public:
	inline void ResetCypherBlock()
	{
		Cypher.setKey(KeyHolder, sizeof(KeyHolder));
		Cypher.setIV(IVHolder, sizeof(IVHolder));
		Cypher.addAuthData(TokenHolder, sizeof(TokenHolder));
	}

	//Returns 8 bit MAC/CRC.
	uint8_t Encode(uint8_t* message, const uint8_t messageLength)
	{
		if (CryptoEnable)
		{
			ResetCypherBlock();
			Cypher.encrypt(message, message, messageLength);
		}

		CalculatorCRC.Reset();

		return CalculatorCRC.Update(message, messageLength);
	}

	//Returns 8 bit MAC/CRC.
	uint8_t Encode(uint8_t* message, const uint8_t messageLength, uint8_t* outputMessage)
	{
		if (CryptoEnable)
		{
			ResetCypherBlock();
			Cypher.encrypt(outputMessage, message, messageLength);
		}
		else
		{
			memcpy(outputMessage, message, messageLength);
		}

		CalculatorCRC.Reset();

		return CalculatorCRC.Update(outputMessage, messageLength);
	}

	//Returns 8 bit MAC/CRC.
	uint8_t Decode(uint8_t* message, const uint8_t messageLength)
	{
		CalculatorCRC.Reset();
		CalculatorCRC.Update(message, messageLength);

		if (CryptoEnable)
		{
			ResetCypherBlock();
			Cypher.decrypt(message, message, messageLength);
		}

		return CalculatorCRC.GetCurrent();
	}

	void EncodeDirect(uint8_t* message, const uint8_t messageLength)
	{
		ResetCypherBlock();
		Cypher.encrypt(message, message, messageLength);
	}

	void DecodeDirect(uint8_t* inputMessage, const uint8_t messageLength, uint8_t* outputMessage)
	{
		ResetCypherBlock();
		Cypher.decrypt(outputMessage, inputMessage, messageLength);
	}

	void Clear()
	{
		EncoderState = StageEnum::AllClear;
		CryptoEnable = false;

		for (uint8_t i = 0; i < sizeof(TokenHolder); i++)
		{
			TokenHolder[i] = 0;
		}

		TokenSeed = 0;
	}

	bool SetEnabled()
	{
		CryptoEnable = true;

		ResetCypherBlock();
	}

	bool IsReadyForUse()
	{
		return EncoderState == StageEnum::AllReady;
	}

	bool SetSecretKey(uint8_t * secretKey, const uint8_t keyLength)
	{
		Cypher.clear();

		//HKey reduction, only use keySize bytes for key (16).
		for (uint8_t i = 0; i < Cypher.keySize(); i++)
		{
			KeyHolder[i] = secretKey[i];
		}

		//Test if key is accepted.
		if (!Cypher.setKey(KeyHolder, Cypher.keySize()))
		{
			return false;
		}

		//Test setting IV.
		if (!Cypher.setIV(IVHolder, 16))
		{
			return false;
		}

		//Key expansion: don't throw away unused key bytes.
		//Update token seed from last 4 bytes of the key.
		TokenSeed = 0;
		TokenSeed += (KeyHolder[keyLength - 4] >> 0);
		TokenSeed += (KeyHolder[keyLength - 3] << 8);
		TokenSeed += (KeyHolder[keyLength - 2] << 16);
		TokenSeed += (KeyHolder[keyLength - 1] << 24);

		ResetCypherBlock();

		EncoderState = StageEnum::AllReady;

		return EncoderState == StageEnum::AllReady;
	}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print("Ascon128");
	}

	void PrintKey(Stream* serial)
	{
		serial->print("Secret Key:\n\t|");
		for (uint8_t i = 0; i < Cypher.keySize(); i++)
		{
			serial->print(KeyHolder[i]);
			serial->print('|');
		}
	}
#endif

	void SetIvData(const uint32_t session, const uint32_t id1, const uint32_t id2)
	{
		//Custom key expansion.
		//TODO: Replace with RFC HKDF.

		//Id 1 with Session.
		Hasher.clear();
		ATUI.uint = session;
		Hasher.update(ATUI.array, sizeof(uint8_t));
		ATUI.uint = id1;
		Hasher.update(ATUI.array, sizeof(uint32_t));
		Hasher.finalize(ATUI.array, sizeof(uint32_t));
		IVHolder[0] = ATUI.array[0];
		IVHolder[1] = ATUI.array[1];
		IVHolder[2] = ATUI.array[2];
		IVHolder[3] = ATUI.array[3];

		//Id 2 with Session.
		Hasher.clear();
		ATUI.uint = session;
		Hasher.update(ATUI.array, sizeof(uint32_t));
		ATUI.uint = id2;
		Hasher.update(ATUI.array, sizeof(uint32_t));
		Hasher.finalize(ATUI.array, sizeof(uint32_t));
		IVHolder[4] = ATUI.array[0];
		IVHolder[5] = ATUI.array[1];
		IVHolder[6] = ATUI.array[2];
		IVHolder[7] = ATUI.array[3];

		//Id 1 and 2 with Session.
		Hasher.clear();
		ATUI.uint = session;
		Hasher.update(ATUI.array, sizeof(uint32_t));
		ATUI.uint = id1;
		Hasher.update(ATUI.array, sizeof(uint32_t));
		ATUI.uint = id2;
		Hasher.update(ATUI.array, sizeof(uint32_t));
		Hasher.finalize(ATUI.array, sizeof(uint32_t));
		IVHolder[8] = ATUI.array[0];
		IVHolder[9] = ATUI.array[1];
		IVHolder[10] = ATUI.array[2];
		IVHolder[11] = ATUI.array[3];

		//Session only.
		Hasher.clear();
		ATUI.uint = session;
		Hasher.update(ATUI.array, sizeof(uint32_t));
		Hasher.finalize(ATUI.array, sizeof(uint32_t));
		IVHolder[12] = ATUI.array[0];
		IVHolder[13] = ATUI.array[1];
		IVHolder[14] = ATUI.array[2];
		IVHolder[15] = ATUI.array[3];
	}

	uint32_t GetSeed()
	{
		return TokenSeed;
	}

	void SetToken(const uint32_t token)
	{
		TokenHolder[0] = token & 0xFF;
		TokenHolder[1] = (token >> 8) & 0xFF;
		TokenHolder[2] = (token >> 16) & 0xFF;
		TokenHolder[3] = (token >> 24) & 0xFF;
	}
};
#endif