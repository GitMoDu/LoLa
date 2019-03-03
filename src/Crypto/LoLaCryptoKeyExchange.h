// LoLaCryptoKeyExchange.h

#ifndef _LOLA_CRYPTO_KEY_EXCHANGE_h
#define _LOLA_CRYPTO_KEY_EXCHANGE_h

#include <uECC.h>

#define CRYPTO_ENTROPY_SOURCE_ANALOG_PIN	PB0

#define LOLA_LINK_CRYPTO_KEY_LENGTH			20 //Derived from selected curve.
#define LOLA_LINK_CRYPTO_SIGNATURE_LENGTH	(2 * LOLA_LINK_CRYPTO_KEY_LENGTH)

class ICryptoKeyExchanger
{
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

protected:
	enum PairingStageEnum : uint8_t
	{
		StageClear,
		StageLocalKey,
		StagePartnerKey,
		StageSharedKey,
		StageReadyForUse,
	} PairingStage = PairingStageEnum::StageClear;

	const struct uECC_Curve_t * ECC_CURVE = uECC_secp160r1();
	static const uint8_t KEY_CURVE_SIZE = LOLA_LINK_CRYPTO_KEY_LENGTH; //160 bits take 20 bytes.

	uint8_t PartnerPublicKey[KEY_CURVE_SIZE + 1];
	uint8_t LocalPublicKey[KEY_CURVE_SIZE + 1];
	uint8_t PrivateKey[KEY_CURVE_SIZE];
	uint8_t SharedKey[KEY_CURVE_SIZE];

	//Temporary holder for the uncompressed public keys.
	uint8_t UncompressedKey[(KEY_CURVE_SIZE * 2) + 1];

protected:
	virtual bool OnFinalize() { return true; }
	virtual bool HasSharedKey() { return false; }

public:
	bool Setup(const uint8_t entropySourcePin = CRYPTO_ENTROPY_SOURCE_ANALOG_PIN)
	{
		uECC_set_rng(&RNG);

		return true;
	}

	bool IsReadyToUse()
	{
		return PairingStage == PairingStageEnum::StageReadyForUse;
	}

	///Make sure there is room for KEY_SIZE bytes in the destination array.
	bool GetSharedKey(uint8_t * keyTarget)
	{
		if (!HasSharedKey())
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

	bool Finalize()
	{
		if (PairingStage != PairingStageEnum::StageSharedKey)
		{
			return false;
		}

		if (PairingStage == PairingStageEnum::StageReadyForUse)
		{
			return true;
		}

		//Restore public key to decompressed, ready to use.
		uECC_decompress(LocalPublicKey, UncompressedKey, ECC_CURVE);

		if (OnFinalize())
		{
			PairingStage = PairingStageEnum::StageReadyForUse;
		}

		return IsReadyToUse();
	}

	bool GetPublicKey(uint8_t * keyTarget)
	{
		if (PairingStage == PairingStageEnum::StageClear)
		{
			return false;
		}

		for (uint8_t i = 0; i < KEY_CURVE_SIZE; i++)
		{
			keyTarget[i] = LocalPublicKey[i];
		}

		return true;
	}

	bool SetPartnerPublicKey(uint8_t* keySource)
	{
		switch (PairingStage)
		{
		case PairingStageEnum::StageClear:
		case PairingStageEnum::StageReadyForUse:
		case PairingStageEnum::StageSharedKey:
			return false;
		default:
			break;
		}

		for (uint8_t i = 0; i < KEY_CURVE_SIZE; i++)
		{
			PartnerPublicKey[i] = keySource[i];
		}

		PairingStage = PairingStageEnum::StagePartnerKey;

		return true;
	}

	bool GenerateNewKeyPair()
	{
		if (uECC_make_key(UncompressedKey, PrivateKey, ECC_CURVE))
		{
			uECC_compress(UncompressedKey, LocalPublicKey, ECC_CURVE);

			PairingStage = PairingStageEnum::StageLocalKey;

			return true;
		}

		return false;
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

class RemoteCryptoKeyExchanger : public ICryptoKeyExchanger
{
protected:
	bool HasSharedKey()
	{
		return PairingStage == PairingStageEnum::StageReadyForUse;
	}

	bool OnFinalize() 
	{ 
		//TODO: Decode Shared key with local private key.

		return true; 
	}

public:
	bool SetEncodedSharedKey(uint8_t* keySource)
	{
		if (PairingStage != PairingStageEnum::StagePartnerKey)
		{
			return false;
		}

		for (uint8_t i = 0; i < KEY_CURVE_SIZE; i++)
		{
			SharedKey[i] = keySource[i];
		}

		PairingStage = PairingStageEnum::StageSharedKey;

		return true;
	}
};
class HostCryptoKeyExchanger : public ICryptoKeyExchanger
{
protected:
	bool HasSharedKey()
	{
		return PairingStage == PairingStageEnum::StageSharedKey || PairingStage == PairingStageEnum::StageReadyForUse;
	}

	bool OnFinalize()
	{
		//TODO: Decode Shared key with local private key.

		return true;
	}

public:
	bool GenerateSharedKey()
	{
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

		uECC_decompress(PartnerPublicKey, UncompressedKey, ECC_CURVE);

		if (uECC_shared_secret(UncompressedKey, PrivateKey, SharedKey, ECC_CURVE) > 0)
		{
			PairingStage = PairingStageEnum::StageSharedKey;
		}

		return PairingStage == PairingStageEnum::StageSharedKey;
	}
};
#endif

