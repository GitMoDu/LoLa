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
		HasSharedKey,
		ReadyForUse
	} EncoderState = StageEnum::AllClear;
private:
	Ascon128 Cypher;

public:
	bool Encode(const uint8_t * message, const uint8_t messageLength, uint8_t * encryptedOutput)
	{
		if (EncoderState != StageEnum::ReadyForUse)
		{
			return false;
		}

		Cypher.encrypt(encryptedOutput, message, messageLength);

		return true;
	}

	bool Decode(const uint8_t * encryptedMessage, const uint8_t messageLength, uint8_t * messageOutput)
	{
		if (EncoderState != StageEnum::ReadyForUse)
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

	inline bool IsReadyForUse()
	{
		return EncoderState == StageEnum::ReadyForUse;
	}

	bool SetSharedKey(const uint8_t * sharedKey, const uint8_t keyLength)	
	{
		if (keyLength < Cypher.keySize())
		{
			return false;
		}

		//TODO: Handle key reduction?

		Cypher.clear();

		if (!Cypher.setKey(sharedKey, keyLength))
		{
			return false;
		}	

		EncoderState = StageEnum::HasSharedKey;

		return true;
	}

	bool SetIv(const uint8_t * iv, const uint8_t ivLength)
	{
		if (EncoderState != StageEnum::HasSharedKey && 
			EncoderState != StageEnum::ReadyForUse)
		{
			return false; //Wrong state
		}

		if (ivLength < Cypher.keySize())
		{
			return false;
		}

		//TODO: Handle key reduction?

		Cypher.clear();

		if (!Cypher.setIV(iv, ivLength))
		{
			return false;
		}

		//Do not set IV yet.		

		EncoderState = StageEnum::HasSharedKey;

		return true;
	}
};
#endif