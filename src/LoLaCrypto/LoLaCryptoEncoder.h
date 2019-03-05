// LoLaCryptoEncoder.h

#ifndef _LOLACRYPTOENCODER_h
#define _LOLACRYPTOENCODER_h

#include <Crypto.h>
#include <CryptoLW.h>
#include <Ascon128.h>
#include <Acorn128.h>
#include "utility/ProgMemUtil.h"

#include <LoLaCrypto\TinyCRC.h>


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

	uint8_t PinHolder[4] = { 0, 0, 0, 0 };
	uint8_t TestData[2] = { 0 , 0 };


private:
	Ascon128 Cypher;
	TinyCrcModbus8 Hasher;

private:
	inline void ResetCypherBlock()
	{
		Cypher.setKey(KeyHolder, 16);
		Cypher.setIV(IVHolder, 16);
		Cypher.addAuthData(PinHolder, 1);
	}

public:
	void Encode(uint8_t* message, const uint8_t messageLength)
	{
		Cypher.encrypt(message, message, messageLength);

		ResetCypherBlock();
	}

	void Decode(uint8_t * message, const uint8_t messageLength)
	{
		Cypher.decrypt(message, message, messageLength);

		ResetCypherBlock();
	}

	void Decode(uint8_t * inputMessage, const uint8_t messageLength, uint8_t* outputMessage)
	{
		Cypher.decrypt(outputMessage, inputMessage, messageLength);
	}

	void Clear()
	{
		EncoderState = StageEnum::AllClear;
	}

	bool IsReadyForUse()
	{
		return EncoderState == StageEnum::AllReady;
	}

	bool SetSecretKey(uint8_t * secretKey, const uint8_t keyLength)
	{
		Cypher.clear();

		//TODO: Handle key reduction?
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

		//Test encoding decoding.
		ResetCypherBlock();
		TestData[0] = random(0,UINT8_MAX);
		TestData[1] = TestData[0];
		Cypher.encrypt(TestData, TestData, 1);
		ResetCypherBlock();
		Cypher.decrypt(TestData, TestData, 1);

		ResetCypherBlock();

		if (TestData[0] != TestData[1])
		{
			return false;
		}

		EncoderState = StageEnum::AllReady;

#ifdef DEBUG_LOLA_CRYPTO
		Serial.print("Secret Key:\n\t|");
		for (uint8_t i = 0; i < Cypher.keySize(); i++)
		{
			Serial.print(KeyHolder[i]);
			Serial.print('|');
		}
		Serial.println();
#endif // DEBUG_LOLA_CRYPTO		

		return EncoderState == StageEnum::AllReady;
	}

	void SetIvData(const uint8_t sessionId, const uint32_t localId, const uint32_t partnerId)
	{
		Hasher.Reset();
		Hasher.Update(sessionId);

		IVHolder[0] = Hasher.Update(localId & 0xff);
		IVHolder[1] = Hasher.Update(sessionId);
		IVHolder[2] = Hasher.Update((localId >> 8) & 0xff);
		IVHolder[3] = Hasher.Update(sessionId);
		IVHolder[4] = Hasher.Update((localId >> 16) & 0xff);
		IVHolder[5] = Hasher.Update(sessionId);
		IVHolder[6] = Hasher.Update((localId >> 24) & 0xff);
		IVHolder[7] = Hasher.Update(sessionId);

		IVHolder[8] = Hasher.Update(partnerId & 0xff);
		IVHolder[9] = Hasher.Update(sessionId);
		IVHolder[10] = Hasher.Update((partnerId >> 8) & 0xff);
		IVHolder[11] = Hasher.Update(sessionId);
		IVHolder[12] = Hasher.Update((partnerId >> 16) & 0xff);
		IVHolder[13] = Hasher.Update(sessionId);
		IVHolder[14] = Hasher.Update((partnerId >> 24) & 0xff);
		IVHolder[15] = Hasher.Update(sessionId);
	}

	bool SetAuthData(const uint8_t* pin, const uint8_t pinLength)
	{
		if (pinLength != 4) //TODO: Pin.
		{
			return false; //Wrong state
		}

		for (uint8_t i = 0; i < pinLength; i++)
		{
			PinHolder[i] = pin[i];
		}

		return true;
	}
};
#endif