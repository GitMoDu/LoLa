// LoLaCryptoEncoder.h

#ifndef _LOLACRYPTOENCODER_h
#define _LOLACRYPTOENCODER_h

#include <Crypto.h>
#include <CryptoLW.h>
#include <Ascon128.h>
#include "utility/ProgMemUtil.h"

class LoLaCryptoEncoder
{
private:
	enum  StageEnum : uint8_t
	{
		AllClear,
		HasSecretKey,
		AllDone
	} EncoderState = StageEnum::AllClear;

	const static uint8_t AuthorizationDataSize = 4;
	const uint8_t AuthorizationData[AuthorizationDataSize] = { 0, 1 ,2 ,3 };
private:
	Acorn128 Cypher;

public:
	bool Encode(const uint8_t * message, const uint8_t messageLength, uint8_t * encryptedOutput)
	{
		if (EncoderState != StageEnum::AllDone)
		{
			return false;
		}

		Cypher.encrypt(encryptedOutput, message, messageLength);

		return true;
	}

	bool Decode(const uint8_t * encryptedMessage, const uint8_t messageLength, uint8_t * messageOutput)
	{
		if (EncoderState != StageEnum::AllDone)
		{
			return false;
		}

		Cypher.decrypt(messageOutput, encryptedMessage, messageLength);

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

	bool SetSecretKey(uint8_t * sharedKey, const uint8_t keyLength)
	{
		Cypher.clear();

		//TODO: Handle key reduction?

		if (!Cypher.setKey(sharedKey, keyLength))
		{
			return false;
		}

		EncoderState = StageEnum::HasSecretKey;

		return true;
	}

	bool SetIv(uint8_t * iv, const uint8_t ivLength)
	{
		if (EncoderState != StageEnum::HasSecretKey)
		{
			return false; //Wrong state
		}

		//TODO: Handle key reduction?
		if (!Cypher.setIV(iv, ivLength))
		{
			return false;
		}

		//TODO: Use authorization data?
		for (uint8_t i = 0; i < AuthorizationDataSize; i++)
		{
			Cypher.addAuthData(&AuthorizationData[i], sizeof(uint8_t));
		}


		EncoderState = StageEnum::AllDone;

		return true;
	}
};
#endif