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

static const uint8_t EmptyKey[LoLaCryptoDefinition::MAC_KEY_SIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t CounterZero[1] = { 0 };

class LoLaCryptoSession : public LoLaLinkSession
{
protected:
	Poly1305 CryptoHasher; // HMAC and Signature hasher.

	uint8_t ProtocolId[LoLaLinkDefinition::PROTOCOL_ID_SIZE]{};

protected:
	uint8_t Nonce[LoLaCryptoDefinition::MAC_KEY_SIZE]{}; // Reusable nonce for CryptoHasher.

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
	///// <summary>
	///// HKDF Expanded key, with extra seeds.
	///// </summary>
	LoLaLinkDefinition::ExpandedKeyStruct* ExpandedKey;

	/// <summary>
	/// Access control password.
	/// </summary>
	const uint8_t* AccessPassword;

private:
	uint8_t MatchToken[LoLaLinkDefinition::SESSION_TOKEN_SIZE]{};

	/// <summary>
	/// Ephemeral session matching token.
	/// </summary>
	uint8_t SessionToken[LoLaLinkDefinition::SESSION_TOKEN_SIZE]{};

private:
	uint8_t LocalChallengeCode[LoLaCryptoDefinition::CHALLENGE_CODE_SIZE]{};
	uint8_t PartnerChallengeCode[LoLaCryptoDefinition::CHALLENGE_CODE_SIZE]{};
	uint8_t PartnerChallengeSignature[LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE]{};

public:
	/// <summary>
	/// </summary>
	/// <param name="expandedKey">sizeof = LoLaLinkDefinition::HKDFSize</param>
	/// <param name="accessPassword">sizeof = LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE</param>
	LoLaCryptoSession(LoLaLinkDefinition::ExpandedKeyStruct* expandedKey, const uint8_t* accessPassword)
		: LoLaLinkSession()
		, CryptoHasher()
		, ExpandedKey(expandedKey)
		, AccessPassword(accessPassword)
	{}

	virtual const bool Setup()
	{
		return ExpandedKey != nullptr && AccessPassword != nullptr;
	}

	void GenerateProtocolId(
		const LoLaLinkDefinition::LinkType linkType,
		const uint16_t duplexPeriod,
		const uint32_t hopperPeriod,
		const uint32_t transceiverCode)
	{
		Nonce[0] = LoLaLinkDefinition::LOLA_VERSION;
		for (uint_fast8_t i = 1; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = 0;
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
		ClearCachedSessionToken();
	}

public:
	void SetRandomSessionId(LoLaRandom* randomSource)
	{
		randomSource->GetRandomStreamCrypto(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
		ClearCachedSessionToken();
	}

	void GetChallengeSignature(const uint8_t* challenge, const uint8_t* password, uint8_t* signatureTarget)
	{
		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(challenge, LoLaCryptoDefinition::CHALLENGE_CODE_SIZE);
		CryptoHasher.update(password, LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE);

		CopySessionIdTo(Nonce);
		for (uint_fast8_t i = LoLaLinkDefinition::SESSION_ID_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = 0;
		}
		CryptoHasher.finalize(Nonce, signatureTarget, LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE);
		CryptoHasher.clear();
	}

	/// <summary>
	/// Populate crypto keys with HKDF from secret key.
	/// N-Bytes key expander sourcing key and session id.
	/// </summary>
	/// <param name="key"></param>
	void CalculateExpandedKey(const uint8_t* key)
	{
		uint8_t index = 0;

		CopySessionIdTo(Nonce);
		for (uint_fast8_t i = LoLaLinkDefinition::SESSION_ID_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = 0;
		}

		while (index < LoLaLinkDefinition::HKDFSize)
		{
			CryptoHasher.reset(EmptyKey);
			CryptoHasher.update(&index, sizeof(uint8_t));
			CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
			CryptoHasher.update(key, LoLaCryptoDefinition::CYPHER_KEY_SIZE);

			if (LoLaLinkDefinition::HKDFSize - index > LoLaCryptoDefinition::MAC_KEY_SIZE)
			{
				CryptoHasher.finalize(Nonce, &((uint8_t*)ExpandedKey)[index], LoLaCryptoDefinition::MAC_KEY_SIZE);
				index += LoLaCryptoDefinition::MAC_KEY_SIZE;
			}
			else
			{
				CryptoHasher.finalize(Nonce, &((uint8_t*)ExpandedKey)[index], LoLaLinkDefinition::HKDFSize - index);
				index += (LoLaLinkDefinition::HKDFSize - index);
			}
		}

		CryptoHasher.clear();
	}

	/// <summary>
	/// </summary>
	/// <param name="localPublicKey"></param>
	/// <param name="partnerPublicKey"></param>
	void CalculateSessionAddressing(const uint8_t* localPublicKey, const uint8_t* partnerPublicKey)
	{
		CryptoHasher.reset(EmptyKey); // 16 byte Auth Key.
		CryptoHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
		CryptoHasher.update(localPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);

		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::ADDRESS_SEED_SIZE; i++)
		{
			Nonce[i] = ExpandedKey->AdressingSeed[i];
		}
		for (uint_fast8_t i = LoLaLinkDefinition::ADDRESS_SEED_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = 0;
		}
		CryptoHasher.finalize(Nonce, InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);

		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(localPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
		CryptoHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
		CryptoHasher.finalize(Nonce, OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
		CryptoHasher.clear();
	}

	void CalculateSessionToken(const uint8_t* partnerPublicKey)
	{
		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);

		CopySessionIdTo(Nonce);
		for (uint_fast8_t i = LoLaLinkDefinition::SESSION_ID_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = 0;
		}
		CryptoHasher.finalize(Nonce, SessionToken, LoLaLinkDefinition::SESSION_TOKEN_SIZE);
		CryptoHasher.clear();
	}

	/// <summary>
	/// Assumes SessionId has been set.
	/// </summary>
	/// <param name="partnerPublicKey"></param>
	/// <returns>True if the calculated secrets are already set for this SessionId and PublicKey</returns>
	const bool SessionTokenMatches(const uint8_t* partnerPublicKey)
	{
		CryptoHasher.reset(EmptyKey);
		CryptoHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);

		CopySessionIdTo(Nonce);
		for (uint_fast8_t i = LoLaLinkDefinition::SESSION_TOKEN_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
		{
			Nonce[i] = 0;
		}
		CryptoHasher.finalize(Nonce, MatchToken, LoLaLinkDefinition::SESSION_TOKEN_SIZE);
		CryptoHasher.clear();

		return CachedSessionTokenMatches(MatchToken);
	}

	void CopyLinkingTokenTo(uint8_t* target)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
		{
			target[i] = ExpandedKey->LinkingToken[i];
		}
	}

	const bool LinkingTokenMatches(const uint8_t* linkingToken)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
		{
			if (linkingToken[i] != ExpandedKey->LinkingToken[i])
			{
				return false;
			}
		}
		return true;
	}

	const bool CachedSessionTokenMatches(const uint8_t* matchToken)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_TOKEN_SIZE; i++)
		{
			if (matchToken[i] != SessionToken[i])
			{
				return false;
			}
		}

		return true;
	}

	void ClearCachedSessionToken()
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
		{
			MatchToken[i] = 0;
		}
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
};
#endif