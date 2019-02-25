// LoLaCryptoKeyExchange.h

#ifndef _LOLA_CRYPTO_KEY_EXCHANGE_h
#define _LOLA_CRYPTO_KEY_EXCHANGE_h

#include <uECC.h>

#define CRYPTO_ENTROPY_SOURCE_ANALOG_PIN	PB0

#define LOLA_LINK_CRYPTO_KEY_LENGTH			24 //Derived from selected curve.


class LoLaCryptoKeyExchange
{
private:
	enum PairingStageEnum : uint8_t
	{
		Clear,
		HasLocalKey,
		HasPartnerKey,
		HasSharedKey
	} PairingStage = PairingStageEnum::Clear;


	const struct uECC_Curve_t * ECC_CURVE = uECC_secp192r1();
	static const uint8_t KEY_SIZE = 24; //192 bits take 24 bytes.
	//uECC_Curve_t* ECCCurve = nullptr;

	uint8_t PublicKey[KEY_SIZE];
	uint8_t PartnerPublicKey[KEY_SIZE];

	uint8_t PrivateKey[KEY_SIZE+1];
	uint8_t SharedKey[KEY_SIZE];

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
				int init = analogRead(CRYPTO_ENTROPY_SOURCE_ANALOG_PIN);
				int count = 0;
				while (analogRead(CRYPTO_ENTROPY_SOURCE_ANALOG_PIN) == init) {
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

public:
	bool Setup(const uint8_t entropySourcePin = CRYPTO_ENTROPY_SOURCE_ANALOG_PIN)
	{
		for (uint8_t i = 0; i < KEY_SIZE; i++)
		{
			PublicKey[i] = 0;
			PartnerPublicKey[i] = 0;
			PrivateKey[i] = 0;
			SharedKey[i] = 0;
		}
		PrivateKey[KEY_SIZE] = 0;

		uECC_set_rng(&RNG);

		return true;
	}

	///Make sure there is room for KEY_SIZE bytes in the destination array.
	bool GetSharedKey(uint8_t * keyTarget)
	{
		if (PairingStage != PairingStageEnum::HasSharedKey)
		{
			return false;
		}

		for (uint8_t i = 0; i < KEY_SIZE; i++)
		{
			keyTarget[i] = SharedKey[i];
		}

		return true;
	}

	bool SetPartnerPublicKey(uint8_t* keySource, const uint8_t length)
	{
		if (PairingStage == PairingStageEnum::Clear)
		{
			return false;
		}

		if (length != KEY_SIZE)
		{
			return false;
		}

		for (uint8_t i = 0; i < KEY_SIZE; i++)
		{
			PartnerPublicKey[i] = keySource[i];
		}

		PairingStage = PairingStageEnum::HasPartnerKey;
	}

	bool GenerateSharedKey()
	{
		if (PairingStage == PairingStageEnum::HasPartnerKey)
		{
			return false;
		}

		if (PairingStage == PairingStageEnum::HasSharedKey)
		{
			//Already generated.
			return true;
		}

		if (uECC_shared_secret(PartnerPublicKey, PrivateKey, SharedKey, ECC_CURVE) > 0)
		{
			PairingStage = PairingStageEnum::HasSharedKey;
			Serial.println("New Shared Key generated");
			return true;
		}

		return false;
	}

	void GenerateNewKeyPair()
	{
		uECC_make_key(PublicKey, PrivateKey, ECC_CURVE);

		Serial.println("New Key Pair generated");

		PairingStage = PairingStageEnum::HasLocalKey;		
	}
};
#endif

