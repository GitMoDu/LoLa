// LoLaCryptoEncoder.h

#ifndef _LOLACRYPTOENCODER_h
#define _LOLACRYPTOENCODER_h

#include <Crypto.h>
#include <CryptoLW.h>
#include <Ascon128.h>
#include <Acorn128.h>
#include "utility/ProgMemUtil.h"

#include <FastCRC.h>


#include <Crypto.h>
#include <SHA256.h>

class LoLaCryptoEncoder
{
private:
	enum  StageEnum : uint8_t
	{
		AllClear,
		AllReady,
		FullPower
	} EncoderState = StageEnum::AllClear;

	Ascon128 Cypher;
	SHA256 Hasher;

	union ArrayToUint32 {
		byte array[sizeof(uint32_t)];
		uint32_t uint;
	} ATUI;

	static const uint8_t KeySize = 16; //Should be the same as Cypher.keySize();
	static const uint8_t TokenSize = 4; //Should be the same as Secret Key - Cypher.keySize();

	uint8_t KeyHolder[KeySize];
	uint8_t IVHolder[KeySize];
	uint8_t TokenHolder[TokenSize];

	uint32_t TokenSeed = 0; //Last 4 bytes of key are used for token.


	///CRC validation.
	FastCRC16 CRC16;
	///


private:
	void SetTokenSeed(uint8_t* seedBytes, const uint8_t length)
	{
		Hasher.clear();
		Hasher.update(seedBytes, length);

		Hasher.finalize(ATUI.array, sizeof(uint32_t));

		TokenSeed = ATUI.uint;
	}

public:
	inline void ResetCypherBlock()
	{
		Cypher.setKey(KeyHolder, sizeof(KeyHolder));
		Cypher.setIV(IVHolder, sizeof(IVHolder));
		Cypher.addAuthData(TokenHolder, sizeof(TokenHolder));
	}

	//Returns 8 bit MAC/CRC.
	uint16_t Encode(uint8_t* message, const uint8_t messageLength)
	{
		if (EncoderState == StageEnum::FullPower)
		{
			ResetCypherBlock();
			Cypher.encrypt(message, message, messageLength);
		}

		return CRC16.modbus(message, messageLength);
	}

	//Returns 8 bit MAC/CRC.
	uint16_t Encode(uint8_t* message, const uint8_t messageLength, uint8_t* outputMessage)
	{
		if (EncoderState == StageEnum::FullPower)
		{
			ResetCypherBlock();
			Cypher.encrypt(outputMessage, message, messageLength);
		}
		else
		{
			memcpy(outputMessage, message, messageLength);
		}

		return CRC16.modbus(outputMessage, messageLength);
	}

	//Returns 8 bit MAC/CRC.
	uint8_t Decode(uint8_t* message, const uint8_t messageLength, const uint16_t crc)
	{
		if (crc != CRC16.modbus(message, messageLength))
		{
			return false;
		}

		if (EncoderState == StageEnum::FullPower)
		{
			ResetCypherBlock();
			Cypher.decrypt(message, message, messageLength);
		}

		return true;
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

		for (uint8_t i = 0; i < sizeof(TokenHolder); i++)
		{
			TokenHolder[i] = 0;
		}

		TokenSeed = 0;
	}

	bool SetEnabled()
	{
		if (EncoderState == StageEnum::AllReady)
		{
			EncoderState = StageEnum::FullPower;

			return true;
		}
		else if (EncoderState == StageEnum::FullPower)
		{
			return true;
		}

		return false;
	}

	bool SetSecretKey(uint8_t * secretKey, const uint8_t keyLength)
	{
		if (keyLength < KeySize + TokenSize)
		{
			return false;
		}

		Cypher.clear();

		//HKey reduction, only use keySize bytes for key.
		for (uint8_t i = 0; i < KeySize; i++)
		{
			KeyHolder[i] = secretKey[i];
		}

		//Test if key is accepted.
		if (!Cypher.setKey(KeyHolder, KeySize))
		{
			return false;
		}

		//Test setting IV.
		if (!Cypher.setIV(IVHolder, KeySize))
		{
			return false;
		}

		//Update token seed from last unused bytes of the key.
		SetTokenSeed(&KeyHolder[KeySize], keyLength - KeySize);

		ResetCypherBlock();

		EncoderState = StageEnum::AllReady;

		return true;
	}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print("Ascon128");
	}

	void PrintKey(Stream* serial)
	{
		serial->print("Secret Key:\n\t|");
		for (uint8_t i = 0; i < KeySize; i++)
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

	uint32_t GetToken()
	{
		for (uint8_t i = 0; i < TokenSize; i++)
		{
			ATUI.array[i] = TokenHolder[i];
		}

		return ATUI.uint;
	}

	void SetToken(const uint32_t token)
	{
		ATUI.uint = token;
		Hasher.update(ATUI.array, sizeof(uint32_t));
		Hasher.finalize(ATUI.array, sizeof(uint32_t));

		for (uint8_t i = 0; i < TokenSize; i++)
		{
			TokenHolder[i] = ATUI.array[i];
		}
	}
};
#endif