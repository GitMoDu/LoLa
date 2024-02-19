// LoLaCryptoSession.h

#ifndef _LOLA_CRYPTO_SESSION_h
#define _LOLA_CRYPTO_SESSION_h

#include <IEntropy.h>

#include "..\Link\LoLaLinkSession.h"

#include "LoLaCryptoDefinition.h"
#include "LoLaRandom.h"
#include "SeedXorShifter.h"

#if defined(LOLA_USE_POLY1305)
/*
* https://github.com/rweather/arduinolibs
*/
#include "Poly1305Wrapper.h"
#else
/*
* https://github.com/rweather/lightweight-crypto
*/
#include "XoodyakHashWrapper.h"
#endif

class LoLaCryptoSession : public LoLaLinkSession
{
private:
	/// <summary>
	/// 8 bit fast hasher for deterministic pseudo-random channel.
	/// </summary>
	SeedXorShifter ChannelHasher{};

protected:
	/// <summary>
	/// MAC and Signature hasher.
	/// </summary>
#if defined(LOLA_USE_POLY1305)
	Poly1305Wrapper CryptoHasher{};
#else
	XoodyakHashWrapper<LoLaPacketDefinition::MAC_SIZE> CryptoHasher{};
#endif


protected:
	///// <summary>
	///// HKDF Expanded key, with extra seeds.
	///// </summary>
	LoLaLinkDefinition::ExpandedKeyStruct ExpandedKey{};

	/// <summary>
	/// Reusable nonce for encode/decode. 2 extra bytes to keep the size required by the cypher.
	/// </summary>
	uint8_t Nonce[LoLaCryptoDefinition::CYPHER_IV_SIZE]{};

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

	uint8_t LinkingToken[LoLaLinkDefinition::LINKING_TOKEN_SIZE]{};
private:
	uint8_t LocalChallengeCode[LoLaCryptoDefinition::CHALLENGE_CODE_SIZE]{};
	uint8_t PartnerChallengeCode[LoLaCryptoDefinition::CHALLENGE_CODE_SIZE]{};
	uint8_t PartnerChallengeSignature[LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE]{};

protected:
	virtual const LoLaLinkDefinition::LinkType GetLinkType() { return LoLaLinkDefinition::LinkType::PublicKeyExchange; }

public:
	/// <summary>
	/// </summary>
	/// <param name="accessPassword">sizeof = LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE</param>
	LoLaCryptoSession(const uint8_t* accessPassword)
		: LoLaLinkSession()
		, AccessPassword(accessPassword)
	{}

	const uint8_t GetPrngHopChannel(const uint32_t tokenIndex)
	{
		// Add Token Index and return the result.
		return ChannelHasher.GetHash(tokenIndex);
	}

	virtual const bool Setup()
	{
		return AccessPassword != nullptr
			&& CryptoHasher.DIGEST_LENGTH >= LoLaPacketDefinition::MAC_SIZE;
	}

	/// <summary>
	/// Generate and set the Protocol Id that will bind this Link.
	/// </summary>
	/// <param name="duplexPeriod">Result from IDuplex.</param>
	/// <param name="hopperPeriod">Result from IHop.</param>
	/// <param name="transceiverCode">Result from ILoLaTransceiver.</param>
	void GenerateProtocolId(
		const uint16_t duplexPeriod,
		const uint32_t hopperPeriod,
		const uint32_t transceiverCode)
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(LoLaLinkDefinition::LOLA_VERSION);
		CryptoHasher.update((uint8_t)GetLinkType());
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.update((uint8_t)LoLaLinkDefinition::MacType::Poly1305);
#else
		CryptoHasher.update((uint8_t)LoLaLinkDefinition::MacType::Xoodyak);
#endif
		CryptoHasher.update(duplexPeriod);
		CryptoHasher.update(hopperPeriod);
		CryptoHasher.update(transceiverCode);

#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
#else
		CryptoHasher.finalize(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
#endif
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
	/// N-Bytes key expander sourcing
	///	-	key
	/// -	access password
	///	-	session id
	///	-	protocol id
	/// </summary>
	/// <param name="key"></param>
	/// <param name="keySize"></param>
	void CalculateExpandedKey(const uint8_t* key, const uint8_t keySize)
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
#endif
		uint8_t index = 0;
		while (index < LoLaLinkDefinition::HKDFSize)
		{
#if defined(LOLA_USE_POLY1305)
			CryptoHasher.reset(Nonce);
#else
			CryptoHasher.reset();
#endif
			CryptoHasher.update(index);
			CryptoHasher.update(key, keySize);
			CryptoHasher.update(AccessPassword, LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE);
			CryptoHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
			CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);

			if (LoLaLinkDefinition::HKDFSize - index > CryptoHasher.DIGEST_LENGTH)
			{
#if defined(LOLA_USE_POLY1305)
				CryptoHasher.finalize(Nonce, &((uint8_t*)&ExpandedKey)[index], CryptoHasher.DIGEST_LENGTH);
#else
				CryptoHasher.finalize(&((uint8_t*)&ExpandedKey)[index], CryptoHasher.DIGEST_LENGTH);
#endif
				index += CryptoHasher.DIGEST_LENGTH;
			}
			else
			{
#if defined(LOLA_USE_POLY1305)
				CryptoHasher.finalize(Nonce, &((uint8_t*)&ExpandedKey)[index], LoLaLinkDefinition::HKDFSize - index);
#else
				CryptoHasher.finalize(&((uint8_t*)&ExpandedKey)[index], LoLaLinkDefinition::HKDFSize - index);
#endif
				index += (LoLaLinkDefinition::HKDFSize - index);
			}
		}
		CryptoHasher.clear();

		// Set Hasher with Channel Hop Seed.
		ChannelHasher.SetSeed(ExpandedKey.ChannelSeed);
	}

	/// <summary>
	/// Set the directional Input and Output keys.
	/// </summary>
	/// <param name="localKey">Pointer to array of size = keySize.</param>
	/// <param name="partnerKey">Pointer to array of size = keySize.</param>
	/// <param name="keySize">[0;UINT8_MAX]</param>
	void CalculateSessionAddressing(const uint8_t* localKey, const uint8_t* partnerKey, const uint8_t keySize)
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(ExpandedKey.CypherIvSeed, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
		CryptoHasher.update(partnerKey, keySize);
		CryptoHasher.update(localKey, keySize);
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);

		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.finalize(InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
		CryptoHasher.reset();
#endif
		CryptoHasher.update(ExpandedKey.CypherIvSeed, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
		CryptoHasher.update(localKey, keySize);
		CryptoHasher.update(partnerKey, keySize);
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
#else
		CryptoHasher.finalize(OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
#endif
		CryptoHasher.clear();
	}

	void CalculateLinkingToken(const uint8_t* localKey, const uint8_t* partnerKey, const uint8_t keySize)
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);

		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE; i++)
		{
			CryptoHasher.update((uint8_t)(localKey[i] ^ partnerKey[i]));
		}

#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, LinkingToken, LoLaLinkDefinition::LINKING_TOKEN_SIZE);
#else
		CryptoHasher.finalize(LinkingToken, LoLaLinkDefinition::LINKING_TOKEN_SIZE);
#endif
		CryptoHasher.clear();
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

protected:
	void ClearNonce()
	{
		for (uint_fast8_t i = 0; i < LoLaCryptoDefinition::CYPHER_IV_SIZE; i++)
		{
			Nonce[i] = 0xFF;
		}
	}

private:
	void GetChallengeSignature(const uint8_t* challenge, const uint8_t* password, uint8_t* signatureTarget)
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(password, LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE);
		CryptoHasher.update(challenge, LoLaCryptoDefinition::CHALLENGE_CODE_SIZE);
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, signatureTarget, LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE);
#else
		CryptoHasher.finalize(signatureTarget, LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE);
#endif
		CryptoHasher.clear();
	}
};
#endif