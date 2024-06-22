// LoLaCryptoSession.h

#ifndef _LOLA_CRYPTO_SESSION_h
#define _LOLA_CRYPTO_SESSION_h

#include "../Link/LoLaLinkSession.h"
#include "../Link/LoLaCryptoDefinition.h"
#include "../EntropySources/IEntropy.h"

#include "LoLaRandom.h"
#include "SeedXorShifter.h"

#if defined(LOLA_USE_POLY1305)
/*
* https://github.com/OperatorFoundation/Crypto
* (Arduino fork of https://github.com/rweather/arduinolibs)
*/
#include "Poly1305Wrapper.h"
#else
/*
* https://github.com/rweather/lightweight-crypto
*/
#include "XoodyakHashWrapper.h"
#endif

/// <summary>
/// Cryptographic session features.
/// </summary>
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
	/// <summary>
	/// HKDF Expanded key, with extra seeds.
	/// </summary>
	LoLaCryptoDefinition::ExpandedKeyStruct ExpandedKey{};

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
	uint8_t InputKey[LoLaCryptoDefinition::ADDRESS_KEY_SIZE]{};

	/// <summary>
	/// Implicit addressing Tx key.
	/// Extracted from seed and public keys: [Receiver|Sender]
	/// </summary>
	uint8_t OutputKey[LoLaCryptoDefinition::ADDRESS_KEY_SIZE]{};

protected:
	/// <summary>
	/// Pointer to local public address. sizeof LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE.
	/// </summary>
	const uint8_t* LocalAddress = nullptr;

	/// <summary>
	/// Pointer to access control password. sizeof LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE.
	/// </summary>
	const uint8_t* AccessPassword = nullptr;

	/// <summary>
	/// Pointer to secret key. sizeof LoLaLinkDefinition::SECRET_KEY_SIZE.
	/// </summary>
	const uint8_t* SecretKey = nullptr;

private:
	uint8_t PartnerAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE]{};
	uint8_t LocalChallengeCode[LoLaLinkDefinition::CHALLENGE_CODE_SIZE]{};
	uint8_t PartnerChallengeCode[LoLaLinkDefinition::CHALLENGE_CODE_SIZE]{};
	uint8_t PartnerChallengeSignature[LoLaLinkDefinition::CHALLENGE_SIGNATURE_SIZE]{};

public:
	LoLaCryptoSession()
		: LoLaLinkSession()
	{}

	const uint8_t GetPrngHopChannel(const uint32_t tokenIndex)
	{
		// Add Token Index and return the result.
		return ChannelHasher.GetHash(tokenIndex);
	}

	const bool SetKeys(const uint8_t localAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE],
		const uint8_t accessPassword[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE],
		const uint8_t secretKey[LoLaLinkDefinition::SECRET_KEY_SIZE])
	{
		if (localAddress != nullptr
			&& secretKey != nullptr
			&& accessPassword != nullptr)
		{
			LocalAddress = localAddress;
			SecretKey = secretKey;
			AccessPassword = accessPassword;

			return true;
		}

		return false;
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
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.update((uint8_t)LoLaCryptoDefinition::MacType::Poly1305);
#else
		CryptoHasher.update((uint8_t)LoLaCryptoDefinition::MacType::Xoodyak);
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

public:
	void SetRandomSessionId(LoLaRandom* randomSource)
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
	void CalculateExpandedKey()
	{
		if (AccessPassword == nullptr)
		{
			return;
		}
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
#endif
		uint8_t index = 0;
		while (index < LoLaCryptoDefinition::HKDFSize)
		{
#if defined(LOLA_USE_POLY1305)
			CryptoHasher.reset(Nonce);
#else
			CryptoHasher.reset();
#endif
			CryptoHasher.update(ProtocolId, LoLaLinkDefinition::PROTOCOL_ID_SIZE);
			CryptoHasher.update(index);
			for (uint_fast8_t i = 0; i < LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE; i++)
			{
				CryptoHasher.update((uint8_t)(LocalAddress[i] ^ PartnerAddress[i]));
			}
			CryptoHasher.update(SecretKey, LoLaLinkDefinition::SECRET_KEY_SIZE);
			CryptoHasher.update(AccessPassword, LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE);
			CryptoHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);

			if (LoLaCryptoDefinition::HKDFSize - index > CryptoHasher.DIGEST_LENGTH)
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
				CryptoHasher.finalize(Nonce, &((uint8_t*)&ExpandedKey)[index], LoLaCryptoDefinition::HKDFSize - index);
#else
				CryptoHasher.finalize(&((uint8_t*)&ExpandedKey)[index], LoLaCryptoDefinition::HKDFSize - index);
#endif
				index += (LoLaCryptoDefinition::HKDFSize - index);
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
	void CalculateSessionAddressing()
	{
#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(ExpandedKey.CypherIvSeed, LoLaCryptoDefinition::ADDRESS_KEY_SIZE);
		CryptoHasher.update(PartnerAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
		CryptoHasher.update(LocalAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, InputKey, LoLaCryptoDefinition::ADDRESS_KEY_SIZE);

		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.finalize(InputKey, LoLaCryptoDefinition::ADDRESS_KEY_SIZE);
		CryptoHasher.reset();
#endif
		CryptoHasher.update(ExpandedKey.CypherIvSeed, LoLaCryptoDefinition::ADDRESS_KEY_SIZE);
		CryptoHasher.update(LocalAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
		CryptoHasher.update(PartnerAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, OutputKey, LoLaCryptoDefinition::ADDRESS_KEY_SIZE);
#else
		CryptoHasher.finalize(OutputKey, LoLaCryptoDefinition::ADDRESS_KEY_SIZE);
#endif
		CryptoHasher.clear();
	}

	void GetPairingToken(uint8_t* target)
	{
		if (LocalAddress == nullptr)
		{
			return;
		}

#if defined(LOLA_USE_POLY1305)
		ClearNonce();
		CryptoHasher.reset(Nonce);
#else
		CryptoHasher.reset();
#endif
		CryptoHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);

		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE; i++)
		{
			CryptoHasher.update((uint8_t)(LocalAddress[i] ^ PartnerAddress[i]));
		}

#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, target, LoLaLinkDefinition::LINKING_TOKEN_SIZE);
#else
		CryptoHasher.finalize(target, LoLaLinkDefinition::LINKING_TOKEN_SIZE);
#endif
		CryptoHasher.clear();
	}

	const bool PairingTokenMatches(const uint8_t* pairingToken)
	{
		uint8_t localToken[LoLaLinkDefinition::LINKING_TOKEN_SIZE]{};

		GetPairingToken(localToken);

		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
		{
			if (pairingToken[i] != localToken[i])
			{
				return false;
			}
		}
		return true;
	}

	const bool VerifyChallengeSignature(const uint8_t* signatureSource)
	{
		if (AccessPassword == nullptr)
		{
			return false;
		}

		GetChallengeSignature(LocalChallengeCode, AccessPassword, PartnerChallengeSignature);

		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::CHALLENGE_SIGNATURE_SIZE; i++)
		{
			if (PartnerChallengeSignature[i] != signatureSource[i])
			{
				return false;
			}
		}

		return true;
	}

	void SetPartnerChallenge(const uint8_t challengeSource[LoLaLinkDefinition::CHALLENGE_CODE_SIZE])
	{
		memcpy(PartnerChallengeCode, challengeSource, LoLaLinkDefinition::CHALLENGE_CODE_SIZE);
	}

	void CopyLocalChallengeTo(uint8_t* challengeTarget)
	{
		memcpy(challengeTarget, LocalChallengeCode, LoLaLinkDefinition::CHALLENGE_CODE_SIZE);
	}

	void SignPartnerChallengeTo(uint8_t* signatureTarget)
	{
		if (AccessPassword == nullptr)
		{
			return;
		}
		GetChallengeSignature(PartnerChallengeCode, AccessPassword, signatureTarget);
	}

	void SignLocalChallengeTo(uint8_t* signatureTarget)
	{
		if (AccessPassword == nullptr)
		{
			return;
		}
		GetChallengeSignature(LocalChallengeCode, AccessPassword, signatureTarget);
	}

	void GenerateLocalChallenge(LoLaRandom* randomSource)
	{
		randomSource->GetRandomStreamCrypto(LocalChallengeCode, LoLaLinkDefinition::CHALLENGE_CODE_SIZE);
	}

	const bool AddressCollision()
	{
		if (LocalAddress == nullptr)
		{
			return true;
		}
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE; i++)
		{
			if (LocalAddress[i] != PartnerAddress[i])
			{
				return false;
			}
		}
		return true;
	}

	void CopyLocalAddressTo(uint8_t* target)
	{
		if (LocalAddress == nullptr)
		{
			return;
		}
		memcpy(target, LocalAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
	}

	void CopyPartnerAddressTo(uint8_t* target)
	{
		memcpy(target, PartnerAddress, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
	}

	const bool LocalAddressMatchFrom(const uint8_t* source)
	{
		if (LocalAddress == nullptr)
		{
			return false;
		}

		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE; i++)
		{
			if (LocalAddress[i] != source[i])
			{
				return false;
			}
		}

		return true;
	}

protected:
	void ClearNonce()
	{
		memset(Nonce, 0xFF, LoLaCryptoDefinition::CYPHER_IV_SIZE);
	}

	void SetPartnerAddress(const uint8_t* source)
	{
		memcpy(PartnerAddress, source, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
	}

	void ClearPartnerAddress()
	{
		memset(PartnerAddress, 0, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE);
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
		CryptoHasher.update(challenge, LoLaLinkDefinition::CHALLENGE_CODE_SIZE);
#if defined(LOLA_USE_POLY1305)
		CryptoHasher.finalize(Nonce, signatureTarget, LoLaLinkDefinition::CHALLENGE_SIGNATURE_SIZE);
#else
		CryptoHasher.finalize(signatureTarget, LoLaLinkDefinition::CHALLENGE_SIGNATURE_SIZE);
#endif
		CryptoHasher.clear();
	}
};
#endif