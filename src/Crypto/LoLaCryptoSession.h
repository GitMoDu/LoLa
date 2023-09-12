// LoLaCryptoSession.h

#ifndef _LOLA_CRYPTO_SESSION_h
#define _LOLA_CRYPTO_SESSION_h

#include <IEntropy.h>

#include "LoLaCryptoDefinition.h"
#include "LoLaRandom.h"

#include "..\Link\LoLaLinkSession.h"

/*
* https://github.com/rweather/arduinolibs
*/
#include <Poly1305.h>

/*
* https://github.com/FrankBoesing/FastCRC
*/
#include <FastCRC.h>

static const uint8_t EmptyKey[LoLaCryptoDefinition::MAC_KEY_SIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

class LoLaCryptoSession : public LoLaLinkSession
{
private:
	FastCRC8 FastHasher; // 8 bit fast hasher for deterministic PRNG.

protected:
	/// <summary>
	/// HMAC and Signature hasher.
	/// </summary>
	Poly1305 CryptoHasher;

	///// <summary>
	///// HKDF Expanded key, with extra seeds.
	///// </summary>
	LoLaLinkDefinition::ExpandedKeyStruct ExpandedKey{};

	/// <summary>
	/// Reusable nonce for CryptoHasher.
	/// </summary>
	uint8_t Nonce[LoLaCryptoDefinition::MAC_KEY_SIZE]{};

protected:
	/// <summary>
	/// 32 bit protocol id.
	/// </summary>
	uint8_t ProtocolId[LoLaLinkDefinition::PROTOCOL_ID_SIZE]{};

protected:
	/// <summary>
	/// Implicit addressing Rx key.
	/// Extracted from seed and public keys: [Sender|Receiver]
	/// </summary>
	uint8_t InputKey[LoLaLinkDefinition::ADDRESS_KEY_SIZE]{};

	/// <summary>
	/// Implicit addressing Tx key.
	/// Extracted from seed and public keys: [Receiver|Sender]
	/// </summary>
	uint8_t OutputKey[LoLaLinkDefinition::ADDRESS_KEY_SIZE]{};

protected:
	/// <summary>
	/// Access control password.
	/// </summary>
	const uint8_t* AccessPassword;

private:
	uint8_t LinkingToken[LoLaLinkDefinition::LINKING_TOKEN_SIZE]{};
	uint8_t LocalChallengeCode[LoLaCryptoDefinition::CHALLENGE_CODE_SIZE]{};
	uint8_t PartnerChallengeCode[LoLaCryptoDefinition::CHALLENGE_CODE_SIZE]{};
	uint8_t PartnerChallengeSignature[LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE]{};

public:
	/// <summary>
	/// </summary>
	/// <param name="accessPassword">sizeof = LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE</param>
	LoLaCryptoSession(const uint8_t* accessPassword)
		: LoLaLinkSession()
		, FastHasher()
		, CryptoHasher()
		, AccessPassword(accessPassword)
	{}

	const uint8_t GetPrngHopChannel(const uint32_t tokenIndex)
	{
		// Start Hash with Channel Hop Seed.
		FastHasher.smbus(ExpandedKey.ChannelSeed, LoLaLinkDefinition::CHANNEL_KEY_SIZE);

		// Add Token Index and return the result.
		return FastHasher.smbus_upd((uint8_t*)&tokenIndex, sizeof(uint32_t));
	}

	virtual const bool Setup()
	{
		return AccessPassword != nullptr;
	}

	/// <summary>
	/// Generate and set the Protocol Id that will bind this Link.
	/// </summary>
	/// <param name="linkType">LoLaLinkDefinition::LinkType</param>
	/// <param name="duplexPeriod">Result from IDuplex.</param>
	/// <param name="hopperPeriod">Result from IHop.</param>
	/// <param name="transceiverCode">Result from ILoLaTransceiver.</param>
	void GenerateProtocolId(
		const LoLaLinkDefinition::LinkType linkType,
		const uint16_t duplexPeriod,
		const uint32_t hopperPeriod,
		const uint32_t transceiverCode)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = LoLaLinkDefinition::LOLA_VERSION;
		}

		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update((uint8_t*)&linkType, sizeof(LoLaLinkDefinition::LinkType));
		CryptoHasher.update((uint8_t*)&duplexPeriod, sizeof(uint16_t));
		CryptoHasher.update((uint8_t*)&hopperPeriod, sizeof(uint32_t));
		CryptoHasher.update((uint8_t*)&transceiverCode, sizeof(uint32_t));

		CryptoHasher.finalize(Nonce, ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
		CryptoHasher.clear();

#if defined(DEBUG_LOLA)
		Serial.print(F("Transceiver Code: |"));
		for (size_t i = 0; i < sizeof(uint32_t); i++)
		{
			if (((uint8_t*)&transceiverCode)[i] < 0x10)
			{
				Serial.print(0);
			}
			Serial.print(((uint8_t*)&transceiverCode)[i], HEX);
			Serial.print('|');
		}
		Serial.println();
		Serial.print(F("Protocol Id: |"));
		for (size_t i = 0; i < LoLaLinkDefinition::PROTOCOL_ID_SIZE; i++)
		{
			if (ProtocolId[i] < 0x10)
			{
				Serial.print(0);
			}
			Serial.print(ProtocolId[i], HEX);
			Serial.print('|');
		}
		Serial.println();
		Serial.println();
#endif
	}

	virtual void SetSessionId(const uint8_t* sessionId)
	{
		LoLaLinkSession::SetSessionId(sessionId);
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
		{
			LinkingToken[i] = 0;
		}
	}

public:
	virtual void SetRandomSessionId(LoLaRandom* randomSource)
	{
		randomSource->GetRandomStreamCrypto(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
	}

	/// <summary>
	/// Populate crypto keys with HKDF from secret key.
	/// N-Bytes key expander sourcing key and session id.
	/// </summary>
	/// <param name="key"></param>
	void CalculateExpandedKey(const uint8_t* key, const uint8_t keySize)
	{
		uint8_t index = 0;
		while (index < LoLaLinkDefinition::HKDFSize)
		{
			for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
			{
				Nonce[i] = SessionId[i % LoLaLinkDefinition::SESSION_ID_SIZE] ^ index;
			}

			CryptoHasher.reset(EmptyKey);
			CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
			CryptoHasher.update(AccessPassword, LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE);
			CryptoHasher.update(key, keySize);

			if (LoLaLinkDefinition::HKDFSize - index > LoLaCryptoDefinition::MAC_KEY_SIZE)
			{
				CryptoHasher.finalize(Nonce, &((uint8_t*)&ExpandedKey)[index], LoLaCryptoDefinition::MAC_KEY_SIZE);
				index += LoLaCryptoDefinition::MAC_KEY_SIZE;
			}
			else
			{
				CryptoHasher.finalize(Nonce, &((uint8_t*)&ExpandedKey)[index], LoLaLinkDefinition::HKDFSize - index);
				index += (LoLaLinkDefinition::HKDFSize - index);
			}
		}

		CryptoHasher.clear();
	}

	/// <summary>
	/// Set the directional Input and Output keys.
	/// </summary>
	/// <param name="localKey">Pointer to array of size = keySize.</param>
	/// <param name="partnerKey">Pointer to array of size = keySize.</param>
	/// <param name="keySize">[0;UINT8_MAX]</param>
	void CalculateSessionAddressing(const uint8_t* localKey, const uint8_t* partnerKey, const uint8_t keySize)
	{
		FillNonceClear();

		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(ExpandedKey.AdressingSeed, LoLaLinkDefinition::ADDRESS_SEED_SIZE);
		CryptoHasher.update(partnerKey, keySize);
		CryptoHasher.update(localKey, keySize);
		CryptoHasher.finalize(Nonce, InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);

		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(ExpandedKey.AdressingSeed, LoLaLinkDefinition::ADDRESS_SEED_SIZE);
		CryptoHasher.update(localKey, keySize);
		CryptoHasher.update(partnerKey, keySize);
		CryptoHasher.finalize(Nonce, OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
		CryptoHasher.clear();
	}

	/// <summary>
	/// Hash the linking seed to get a common linking token,
	/// without exposing raw key data.
	/// </summary>
	void CalculateLinkingToken()
	{
		FillNonceClear();

		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(ExpandedKey.LinkingSeed, LoLaLinkDefinition::LINKING_TOKEN_SIZE);
		CryptoHasher.finalize(Nonce, LinkingToken, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
	}

	void CopyLinkingTokenTo(uint8_t* target)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
		{
			target[i] = LinkingToken[i];
		}
	}

	const bool LinkingTokenMatches(const uint8_t* linkingToken)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
		{
			if (linkingToken[i] != LinkingToken[i])
			{
				return false;
			}
		}
		return true;
	}

	const bool VerifyChallengeSignature(const uint8_t* signatureSource)
	{
		GetChallengeSignature(LocalChallengeCode, AccessPassword, PartnerChallengeSignature);

		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE; i++)
		{
			if (PartnerChallengeSignature[i] != signatureSource[i])
			{
				return false;
			}
		}

		return true;
	}

	void SetPartnerChallenge(const uint8_t* challengeSource)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::CHALLENGE_CODE_SIZE; i++)
		{
			PartnerChallengeCode[i] = challengeSource[i];
		}
	}

	void CopyLocalChallengeTo(uint8_t* challengeTarget)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::CHALLENGE_CODE_SIZE; i++)
		{
			challengeTarget[i] = LocalChallengeCode[i];
		}
	}

	void SignPartnerChallengeTo(uint8_t* signatureTarget)
	{
		GetChallengeSignature(PartnerChallengeCode, AccessPassword, signatureTarget);
	}

	void SignLocalChallengeTo(uint8_t* signatureTarget)
	{
		GetChallengeSignature(LocalChallengeCode, AccessPassword, signatureTarget);
	}

	void GenerateLocalChallenge(LoLaRandom* randomSource)
	{
		randomSource->GetRandomStreamCrypto(LocalChallengeCode, LoLaCryptoDefinition::CHALLENGE_CODE_SIZE);
	}

	void FillNonceClear()
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = 0;
		}
	}

private:
	void GetChallengeSignature(const uint8_t* challenge, const uint8_t* password, uint8_t* signatureTarget)
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = challenge[i % LoLaCryptoDefinition::CHALLENGE_CODE_SIZE];
		}

		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(password, LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE);
		CryptoHasher.finalize(Nonce, signatureTarget, LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE);
		CryptoHasher.clear();
	}
};
#endif