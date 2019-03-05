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
		HasSecretKey,
		AllDone
	} EncoderState = StageEnum::AllClear;

	uint8_t IVHolder[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
private:
	Acorn128 Cypher;

	TinyCrcModbus8 Hasher;

public:
	bool Encode(uint8_t* message, const uint8_t messageLength)
	{
		if (EncoderState != StageEnum::AllDone)
		{
			return false;
		}

		Cypher.encrypt(message, message, messageLength);

		return true;
	}

	bool Decode(uint8_t * message, const uint8_t messageLength)
	{
		if (EncoderState != StageEnum::AllDone)
		{
			return false;
		}

		Cypher.decrypt(message, message, messageLength);

		return true;
	}

	bool Decode(uint8_t * inputMessage, const uint8_t messageLength, uint8_t* outputMessage)
	{
		if (EncoderState != StageEnum::AllDone)
		{
			return false;
		}

		Cypher.decrypt(outputMessage, inputMessage, messageLength);

		return true;
	}

	void Clear()
	{
		EncoderState = StageEnum::AllClear;
	}

	bool IsReadyForUse()
	{
		return EncoderState == StageEnum::AllDone;
	}

	bool SetSecretKey(uint8_t * secretKey, const uint8_t keyLength)
	{
		Cypher.clear();

		//TODO: Handle key reduction?

		if (!Cypher.setKey(secretKey, Cypher.keySize()))
		{
			return false;
		}

		EncoderState = StageEnum::HasSecretKey;

		return true;
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
		if (EncoderState != StageEnum::HasSecretKey)
		{
			return false; //Wrong state
		}

		Serial.print("IV: |");
		for (uint8_t i = 0; i < 16; i++)
		{
			Serial.print(IVHolder[i]);
			Serial.print('|');
		}
		Serial.println('|');

		Serial.print("Auth Data: |");
		for (uint8_t i = 0; i < pinLength; i++)
		{
			Serial.print(pin[i]);
			Serial.print('|');
		}
		if(pinLength == 0)
			Serial.println('|');

		if (Cypher.setIV(IVHolder, 16))
		{
			EncoderState = StageEnum::AllDone;
		}

		if (pinLength > 0)
		{
			Cypher.addAuthData(pin, pinLength);
		}		

		return true;
	}
};
#endif