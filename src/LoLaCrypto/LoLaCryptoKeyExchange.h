// LoLaCryptoKeyExchange.h

#ifndef _LOLA_CRYPTO_KEY_EXCHANGE_h
#define _LOLA_CRYPTO_KEY_EXCHANGE_h

#include <uECC.h>

class LoLaCryptoKeyExchanger
{
public:
	static const uint8_t KEY_CURVE_SIZE = 20; //160 bits take 20 bytes.
	static const uint8_t KEY_MAX_SIZE = KEY_CURVE_SIZE + 1; //Enough for compressed public keys.
	static const uint8_t SIGNATURE_LENGTH = (KEY_CURVE_SIZE * 2);

private:
	static int RNG(uint8_t *dest, unsigned size)
	{
		// Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of 
		// random noise). This can take a long time to generate random data if the result of analogRead(0) 
		// doesn't change very frequently.
		uint8_t val = 0;
		while (size) {
			val = 0;
			for (unsigned i = 0; i < 8; ++i) {
				int init = analogRead(LOLA_LINK_ENTROPY_SOURCE_ANALOG_PIN);
				int count = 0;
				while (analogRead(LOLA_LINK_ENTROPY_SOURCE_ANALOG_PIN) == init) {
					++count;
				}

				if (count == 0) {
					val = (val << 1) | (init & 0x01);
				}
				else {
					val = (val << 1) | (count & 0x01);
				}
			}
			*dest = val;
			++dest;
			--size;
		}
		// NOTE: it would be a good idea to hash the resulting random data using SHA-256 or similar.
		return 1;
	}

	enum PairingStageEnum : uint8_t
	{
		StageClear,
		StageLocalKey,
		StagePartnerKey,
		StageSharedKey
	} PairingStage = PairingStageEnum::StageClear;

	const struct uECC_Curve_t * ECC_CURVE = uECC_secp160r1();
	

	uint8_t PartnerPublicKeyCompressed[KEY_CURVE_SIZE + 1];
	uint8_t LocalPublicKeyCompressed[KEY_CURVE_SIZE + 1];
	uint8_t PrivateKey[KEY_CURVE_SIZE + 1];
	uint8_t SharedKey[KEY_CURVE_SIZE];

	//Temporary holder for the uncompressed public keys.
	uint8_t UncompressedKey[KEY_CURVE_SIZE * 2];

public:
	LoLaCryptoKeyExchanger() {}

	bool Setup()
	{
		uECC_set_rng(&RNG);

		PairingStage = PairingStageEnum::StageClear;

		return true;
	}

	void Clear()
	{
		PairingStage = PairingStageEnum::StageClear;
	}

	void ClearPartner()
	{
		if (PairingStage == PairingStageEnum::StagePartnerKey || PairingStage == PairingStageEnum::StageSharedKey)
		{
			PairingStage = PairingStageEnum::StageLocalKey;
		}	
	}

	bool IsReadyToUse()
	{
		return PairingStage == PairingStageEnum::StageSharedKey;
	}

	uint8_t* GetSharedKeyPointer()
	{
		return SharedKey;
	}

	///Make sure there is room for KEY_SIZE bytes in the destination array.
	bool GetSharedKey(uint8_t * keyTarget)
	{
		if (PairingStage != PairingStageEnum::StageSharedKey)
		{
			return false;
		}

		for (uint8_t i = 0; i < KEY_CURVE_SIZE; i++)
		{
			keyTarget[i] = SharedKey[i];
		}

		PairingStage = PairingStageEnum::StageSharedKey;

		return true;
	}

#ifdef DEBUG_LOLA
	bool GetPartnerPublicKeyCompressed(uint8_t * keyTarget)
	{
		for (uint8_t i = 0; i < KEY_CURVE_SIZE + 1; i++)
		{
			keyTarget[i] = PartnerPublicKeyCompressed[i];
		}

		return true;
	}

	void Debug(Stream* serial)
	{
		serial->print(F("uECC SECP160 R1 Public Key Exchange"));
	}
#endif

	bool GetPublicKeyCompressed(uint8_t * keyTarget)
	{
		if (PairingStage == PairingStageEnum::StageClear)
		{
			return false;
		}

		for (uint8_t i = 0; i < KEY_CURVE_SIZE + 1; i++)
		{
			keyTarget[i] = LocalPublicKeyCompressed[i];
		}

		return true;
	}

	bool SetPartnerPublicKey(uint8_t* keySource)
	{
		if (PairingStage != PairingStageEnum::StageLocalKey)
		{
			return false;
		}

		for (uint8_t i = 0; i < KEY_CURVE_SIZE + 1; i++)
		{
			PartnerPublicKeyCompressed[i] = keySource[i];
		}

		PairingStage = PairingStageEnum::StagePartnerKey;

		return true;
	}

	bool GenerateNewKeyPair()
	{
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_ENCRYPTION)
			uint32_t SharedKeyTime = micros();
#endif
		if (uECC_make_key(UncompressedKey, PrivateKey, ECC_CURVE))
		{
			uECC_compress(UncompressedKey, LocalPublicKeyCompressed, ECC_CURVE);

			PairingStage = PairingStageEnum::StageLocalKey;

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_ENCRYPTION)
			SharedKeyTime = micros() - SharedKeyTime;
			Serial.print(F("Keys generation took "));
			Serial.print(SharedKeyTime);
			Serial.println(F(" us."));
#endif

			return true;
		}

		return false;
	}

	bool GenerateSharedKey()
	{
#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_ENCRYPTION)
		uint32_t SharedKeyTime = micros();
#endif
		switch (PairingStage)
		{
		case PairingStageEnum::StagePartnerKey:
			break;
		case PairingStageEnum::StageSharedKey:
			//Already generated.
			return true;
		default:
			return false;
		}

		uECC_decompress(PartnerPublicKeyCompressed, UncompressedKey, ECC_CURVE);

		if (uECC_shared_secret(UncompressedKey, PrivateKey, SharedKey, ECC_CURVE) > 0)
		{
			//Restore public key to decompressed, ready to use.
			uECC_decompress(LocalPublicKeyCompressed, UncompressedKey, ECC_CURVE);

			PairingStage = PairingStageEnum::StageSharedKey;

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_ENCRYPTION)
			SharedKeyTime = micros() - SharedKeyTime;
			Serial.print(F("Shared key generation took "));
			Serial.print(SharedKeyTime);
			Serial.println(F(" us."));
#endif
		}

		return PairingStage == PairingStageEnum::StageSharedKey;
	}

	//Untested. TODO: Test.
	//bool SignMessage(uint8_t * message, const uint8_t size, uint8_t* output)
	//{
	//	if (PairingStage != PairingStageEnum::StageSharedKey)
	//	{
	//		return false;
	//	}

	//	if (!uECC_sign(PrivateKey, message, size, output, ECC_CURVE))
	//	{
	//		return false;
	//	}

	//	return true;
	//}

	//bool ValidateMessageSignature(uint8_t * message, const uint8_t size, uint8_t* signature)
	//{
	//	if (PairingStage != PairingStageEnum::StageSharedKey)
	//	{
	//		return false;
	//	}

	//	//At this pairing stage, we should have the public key decompressed.
	//	if (!uECC_verify(UncompressedKey, message, size, signature, ECC_CURVE))
	//	{
	//		return false;
	//	}

	//	return true;

	//}

};
#endif

