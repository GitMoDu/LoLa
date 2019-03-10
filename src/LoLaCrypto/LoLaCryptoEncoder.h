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

	uint8_t TokenHolder[4] = { 0, 0, 0, 0 };

	uint32_t TokenSeed = 0; //Last 4 bytes of key are used for token.

	bool CryptoEnable = false;


private:
	Ascon128 Cypher;
	TinyCrcModbus8 Hasher;

public:
	void ResetCypherBlock()
	{
		Cypher.setKey(KeyHolder, sizeof(KeyHolder));
		Cypher.setIV(IVHolder, sizeof(IVHolder));
		Cypher.addAuthData(TokenHolder, sizeof(TokenHolder));
	}

	void Encode(uint8_t* message, const uint8_t messageLength)
	{
		if (CryptoEnable)
		{
			Cypher.encrypt(message, message, messageLength);
		}
	}

	void Encode(uint8_t* message, const uint8_t messageLength, uint8_t* outputMessage)
	{
		if (CryptoEnable)
		{
			Cypher.encrypt(outputMessage, message, messageLength);
		}
		else
		{
			memcpy(outputMessage, message, messageLength);
		}
	}

	void Decode(uint8_t* message, const uint8_t messageLength)
	{
		if (CryptoEnable)
		{
			Cypher.decrypt(message, message, messageLength);
		}
	}

	void EncodeDirect(uint8_t* message, const uint8_t messageLength)
	{
		Cypher.encrypt(message, message, messageLength);
		ResetCypherBlock();
	}

	void DecodeDirect(uint8_t* inputMessage, const uint8_t messageLength, uint8_t* outputMessage)
	{
		Cypher.decrypt(outputMessage, inputMessage, messageLength);
		ResetCypherBlock();
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
#ifdef LOLA_USE_ENCRYPTION
		CryptoEnable = true;
#endif
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

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print("Ascon128");
	}
#endif

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