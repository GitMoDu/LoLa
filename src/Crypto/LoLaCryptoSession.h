// LoLaCryptoSession.h

#ifndef _LOLA_CRYPTO_SESSION_h
#define _LOLA_CRYPTO_SESSION_h

#include "..\Link\LoLaLinkDefinition.h"

#include <IEntropySource.h>
#include "LoLaCryptoDefinition.h"
#include "LoLaCryptoPrimitives.h"
#include "LoLaRandom.h"

#include "..\Clock\Timestamp.h"
#include "..\Link\LoLaLinkSession.h"


class LoLaCryptoSession : public LoLaLinkSession
{
private:
	HKDF<LoLaCryptoPrimitives::KeyHashType> KeyExpander; // N-Bytes key expander HKDF.

private:
	LoLaCryptoPrimitives::FastHashType FastHasher; // 32 bit fast hasher for PRNG.

protected:
	LoLaCryptoPrimitives::KeyHashType KeyHasher; // HKDF and Random hasher.

public:
	LoLaRandom RandomSource; // Cryptographic Secure(ish) Random Number Generator.

	/// <summary>
	/// HKDF Expanded key, with extra seeds.
	/// </summary>
	LoLaLinkDefinition::ExpandedKeyStruct ExpandedKey;

	/// <summary>
	/// Implicit addressing Rx key.
	/// Extracted from seed and public keys: [Receiver|Sender]
	/// </summary>
	uint8_t InputKey[LoLaLinkDefinition::ADDRESS_KEY_SIZE];

	/// <summary>
	/// Implicit addressing Tx key.
	/// Extracted from seed and public keys: [Receiver|Sender]
	/// </summary>
	uint8_t OutputKey[LoLaLinkDefinition::ADDRESS_KEY_SIZE];

private:
	uint8_t MatchToken[LoLaLinkDefinition::SESSION_TOKEN_SIZE];

	/// <summary>
	/// Ephemeral session matching token.
	/// </summary>
	uint8_t SessionToken[LoLaLinkDefinition::SESSION_TOKEN_SIZE];

public:
	LoLaCryptoSession(IEntropySource* entropySource)
		: LoLaLinkSession()
		, RandomSource(entropySource)
		, KeyExpander()
		, FastHasher()
		, ExpandedKey()
		, InputKey()
		, OutputKey()
		, SessionToken()
	{}

	virtual const bool Setup()
	{
		return RandomSource.Setup();
	}

public:
	virtual const uint8_t GetPrngHopChannel(const uint32_t tokenIndex) final
	{
		// Start Hash with Token Seed.
		FastHasher.crc32(ExpandedKey.ChannelSeed, LoLaLinkDefinition::CHANNEL_KEY_SIZE);

		// Add Token Index and return the mod of the result.
		return FastHasher.crc32_upd((uint8_t*)&tokenIndex, sizeof(uint32_t)) % UINT8_MAX;
	}

public:
	void SetRandomSessionId()
	{
		RandomSource.GetRandomStreamCrypto(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="key">sizeof = LoLaCryptoDefinition::CYPHER_KEY_SIZE</param>
	void SetSecretKey(const uint8_t* key)
	{
		// Populate crypto keys with HKDF from secret key.
		KeyExpander.setKey((uint8_t*)(key), LoLaCryptoDefinition::CYPHER_KEY_SIZE, SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
	}

	void CalculateExpandedKey()
	{
		// Populate crypto keys with HKDF from secret key.
		KeyExpander.extract(((uint8_t*)(&ExpandedKey)), LoLaLinkDefinition::HKDFSize);

		// Clear hasher from sensitive material. Disabled for performance.
		//KeyExpander.clear();
	}

	/// <summary>
	/// </summary>
	/// <param name="localPublicKey"></param>
	/// <param name="partnerPublicKey"></param>
	void CalculateSessionAddressing(const uint8_t* localPublicKey, const uint8_t* partnerPublicKey)
	{
		KeyHasher.reset(LoLaLinkDefinition::ADDRESS_KEY_SIZE);
		KeyHasher.update(ExpandedKey.AdressingSeed, LoLaLinkDefinition::ADDRESS_SEED_SIZE);
		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
		KeyHasher.update(localPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
		KeyHasher.finalize(InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);

		KeyHasher.reset(LoLaLinkDefinition::ADDRESS_KEY_SIZE);
		KeyHasher.update(ExpandedKey.AdressingSeed, LoLaLinkDefinition::ADDRESS_SEED_SIZE);
		KeyHasher.update(localPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
		KeyHasher.finalize(OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);

		// Clear hasher from sensitive material. Disabled for performance.
		//KeyHasher.clear();
	}

	void CalculateSessionToken(const uint8_t* partnerPublicKey)
	{
		KeyHasher.reset(LoLaLinkDefinition::SESSION_TOKEN_SIZE);
		KeyHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
		KeyHasher.finalize(SessionToken, LoLaLinkDefinition::SESSION_TOKEN_SIZE);

		// Clear hasher from sensitive material. Disabled for performance.
		//KeyHasher.clear();
	}

	/// <summary>
	/// Assumes SessionId has been set.
	/// </summary>
	/// <param name="partnerPublicKey"></param>
	/// <returns>True if the calculated secrets are already set for this SessionId and PublicKey</returns>
	const bool SessionTokenMatches(const uint8_t* partnerPublicKey)
	{
		KeyHasher.reset(LoLaLinkDefinition::SESSION_TOKEN_SIZE);
		KeyHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
		KeyHasher.finalize(MatchToken, LoLaLinkDefinition::SESSION_TOKEN_SIZE);

		// Clear hasher from sensitive material. Disabled for performance.
		//KeyHasher.clear();

		return CachedSessionTokenMatches(MatchToken);
	}

	void CopyLinkingTokenTo(uint8_t* target)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
		{
			target[i] = ExpandedKey.LinkingToken[i];
		}
	}

	const bool LinkingTokenMatches(const uint8_t* linkingToken)
	{
		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
		{
			if (linkingToken[i] != ExpandedKey.LinkingToken[i])
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
};
#endif