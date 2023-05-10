//// AbstractCryptoLoLaLink.h
//
//#ifndef _ABSTRACT_CRYPTO_LOLA_LINK_
//#define _ABSTRACT_CRYPTO_LOLA_LINK_
//
//#include "AbstractLoLaLinkPacket.h"
//#include <IEntropySource.h>
//#include "Crypto\LoLaCryptoEncoderSession.h"
//#include "Crypto\LoLaCryptoDefinition.h"
//#include "Crypto\LoLaRandom.h"
//
//
//template<const uint8_t MaxPayloadLinkSend,
//	const uint8_t MaxPacketReceiveListeners = 10,
//	const uint8_t MaxLinkListeners = 10>
//class AbstractCryptoLoLaLink : public AbstractLoLaLinkPacket<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>
//{
//private:
//	using BaseClass = AbstractLoLaLinkPacket<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>;
//
//
//protected:
//	using BaseClass::InData;
//	using BaseClass::RawInPacket;
//	using BaseClass::RawOutPacket;
//
//public:
//	AbstractCryptoLoLaLink(Scheduler& scheduler,
//		ILoLaPacketDriver* driver,
//		IEntropySource* entropySource,
//		IClockSource* clockSource,
//		ITimerSource* timerSource,
//		IDuplex* duplex,
//		IChannelHop* hop,
//		LoLaLinkSession* session)
//		: BaseClass(scheduler, driver, clockSource, timerSource, duplex, hop, session)
//	{}
//
//	/*virtual const bool Setup()
//	{
//		return Session.Setup() && BaseClass::Setup();
//	}*/
//
//protected:
//	//virtual const uint8_t GetPrngHopChannel(const uint32_t tokenIndex) final
//	//{
//	//	return Session.GetPrngHopChannel(tokenIndex);
//	//}
//
//
//#pragma region Packet
//protected:
//	//void EncodeOutPacket(const uint8_t* data, const uint8_t counter, const uint8_t dataSize)
//	//{
//	//	Session.EncodeOutPacket(data, RawOutPacket, counter, dataSize);
//	//	return;
//	//	//// Plaintext copy of data to output.
//	//	//for (uint_fast8_t i = 0; i < dataSize; i++)
//	//	//{
//	//	//	RawOutPacket[i + LoLaPacketDefinition::DATA_INDEX] = data[i];
//	//	//}
//
//	//	//// Set rolling plaintext rolling counter id.
//	//	//RawOutPacket[LoLaPacketDefinition::ID_INDEX] = counter;
//
//	//	//// Set HMAC without implicit addressing or token.
//	//	//Session.MacOutPacket(RawOutPacket, dataSize);
//	//}
//
//	/// <summary>
//	/// Encodes the data to RawOutPacket.
//	/// </summary>
//	/// <param name="data"></param>
//	/// <param name="timestampSeconds"></param>
//	/// <param name="tokenRoll"></param>
//	/// <param name="counter"></param>
//	/// <param name="dataSize"></param>
//	//void EncodeOutPacket(const uint8_t* data, const uint32_t timestampSeconds, const uint16_t tokenRoll, const uint8_t counter, const uint8_t dataSize)
//	//{
//	//	Session.EncodeOutPacket(data, RawOutPacket, timestampSeconds, tokenRoll, counter, dataSize);
//	//	return;
//	//	///*EncodeOutPacket(data, counter, dataSize);
//	//	//return;*/
//
//	//	///*****************/
//	//	//// Set packet authentication tag.
//	//	//AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = counter;
//	//	//AuthTag[LoLaCryptoDefinition::CYPHER_TAG_SIZE_INDEX] = dataSize;
//	//	//if (LoLaCryptoDefinition::TIME_NONCE_ENABLED)
//	//	//{
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = (uint8_t)(tokenRoll);
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = (uint8_t)(tokenRoll >> 8);
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = (uint8_t)(timestampSeconds);
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = (uint8_t)(timestampSeconds >> 8);
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = (uint8_t)(timestampSeconds >> 16);
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = (uint8_t)(timestampSeconds >> 24);
//	//	//}
//	//	//else
//	//	//{
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = 0;
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = 0;
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = 0;
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = 0;
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = 0;
//	//	//	AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = 0;
//	//	//}
//	//	///*****************/
//
//	//	///*****************/
//	//	//// Write encrypted the counter.
//	//	////RawOutPacket[LoLaPacketDefinition::ID_INDEX] = (tokenRoll % UINT8_MAX) ^ AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX];
//	//	//RawOutPacket[LoLaPacketDefinition::ID_INDEX] = AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX];
//
//	//	//Session.Encrypt(AuthTag, data, RawOutPacket, dataSize);
//
//	//	//// HMAC the packet.
//	//	//Session.HmacOutPacket(AuthTag, RawOutPacket, dataSize);
//	//	/*****************/
//	//}
//#pragma endregion
//};
//
//
/////// <summary>
/////// 
/////// 
/////// CryptoLoLaLink has 3 encoding modes:
///////		- AwaitingLink (Broadcast): No implicit addressing, keys time tokens all zero.
///////		- Linking: Implicit addressing in MAC, time tokens all zero.
///////		- Linked: Implicit addressing in MAC, time tokens rolling.
/////// 
/////// Cryptographic hashes and cyphers depend on https://github.com/rweather/arduinolibs
/////// Lightweight CRC depend on https://github.com/FrankBoesing/FastCRC
/////// </summary>
////template<const uint8_t MaxPayloadLinkSend,
////	const uint8_t MaxPacketReceiveListeners = 10,
////	const uint8_t MaxLinkListeners = 10>
////class AbstractCryptoLoLaLink : public AbstractLoLaLinkPacket<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>
////{
////private:
////	using BaseClass = AbstractLoLaLinkPacket<MaxPayloadLinkSend, MaxPacketReceiveListeners, MaxLinkListeners>;
////
////	using CypherType = ChaCha;
////	using MacHashType = Poly1305;
////	using KeyHashType = BLAKE2s;
////	using FastHashType = FastCRC32;
////
////protected:
////	using BaseClass::InData;
////	using BaseClass::RawInPacket;
////	using BaseClass::RawOutPacket;
////
////private:
////	KeyHashType KeyHasher; // HKDF and Random hasher.
////	HKDF<KeyHashType> KeyExpander; // N-Bytes key expander HKDF.
////
////	CypherType CryptoCypher; // Cryptographic cypher.
////	MacHashType CryptoHasher; // HMAC hasher.
////
////	TokenHashType FastHasher; // 32 bit fast hasher for PRNG.
////
////protected:
////	LoLaRandom<KeyHashType> RandomSource; // Cryptographic Secure(ish) Random Number Generator.
////
////	/// <summary>
////	/// Ephemeral pre-link configuration.
////	/// </summary>
////private:
////	/// <summary>
////	/// Ephemeral session Id.
////	/// </summary>
////	uint8_t SessionId[LoLaLinkDefinition::SESSION_ID_SIZE];
////
////	/// <summary>
////	/// Ephemeral session matching token.
////	/// </summary>
////	uint8_t SessionToken[LoLaLinkDefinition::SESSION_TOKEN_SIZE];
////
////	uint8_t MatchToken[LoLaLinkDefinition::SESSION_TOKEN_SIZE];
////
////	/// <summary>
////	/// Run-time crypto configuration.
////	/// </summary>
////private:
////	/// <summary>
////	/// HKDF Expanded key, with extra seeds.
////	/// </summary>
////	LoLaLinkDefinition::ExpandedKeyStruct ExpandedKey;
////
////	/// <summary>
////	/// Implicit addressing Rx key.
////	/// Extracted from seed and public keys: [Receiver|Sender]
////	/// </summary>
////	uint8_t InputKey[LoLaLinkDefinition::ADDRESS_KEY_SIZE];
////
////	/// <summary>
////	/// Implicit addressing Tx key.
////	/// Extracted from seed and public keys: [Receiver|Sender]
////	/// </summary>
////	uint8_t OutputKey[LoLaLinkDefinition::ADDRESS_KEY_SIZE];
////
////	/// <summary>
////	/// Run-time encoding helpers.
////	/// </summary>
////private:
////	/// <summary>
////	/// Authentication Tag with mixed usage.
////	///  - counter for Cypher.
////	///  - IV and AuthTag for HMAC.
////	/// </summary>
////	uint8_t AuthTag[LoLaCryptoDefinition::MAC_KEY_SIZE];
////
////	uint8_t InputMac[LoLaPacketDefinition::MAC_SIZE];
////
////protected:
////	LoLaCryptoSession CryptoSession;
////
////public:
////	AbstractCryptoLoLaLink(Scheduler& scheduler,
////		ILoLaPacketDriver* driver,
////		IEntropySource* entropySource,
////		IClockSource* clockSource,
////		ITimerSource* timerSource,
////		IDuplex* duplex,
////		IChannelHop* hop)
////		: BaseClass(scheduler, driver, clockSource, timerSource, duplex, hop)
////		, CryptoSession(entropySource)
////		, RandomSource(KeyHasher, entropySource)
////		, CryptoHasher()
////		, KeyExpander()
////		, CryptoCypher(LoLaCryptoDefinition::CYPHER_ROUNDS)
////		, FastHasher()
////		, SessionId()
////		, SessionToken()
////		, ExpandedKey()
////		, InputKey()
////		, OutputKey()
////		, AuthTag()
////		, InputMac()
////	{}
////
////	virtual const bool Setup()
////	{
////		return RandomSource.Setup() &&
////			CryptoCypher.keySize() == LoLaCryptoDefinition::CYPHER_KEY_SIZE &&
////			CryptoCypher.ivSize() == LoLaCryptoDefinition::CYPHER_IV_SIZE &&
////			LoLaCryptoDefinition::MAC_KEY_SIZE >= LoLaCryptoDefinition::CYPHER_TAG_SIZE &&
////			BaseClass::Setup();
////	}
////
////protected:
////	virtual const uint8_t GetPrngHopChannel(const uint32_t tokenIndex) final
////	{
////		// Start Hash with Token Seed.
////		FastHasher.crc32(ExpandedKey.ChannelSeed, LoLaLinkDefinition::CHANNEL_KEY_SIZE);
////
////		// Add Token Index and return the mod of the result.
////		return FastHasher.crc32_upd((uint8_t*)&tokenIndex, sizeof(uint32_t)) % UINT8_MAX;
////	}
////
////#pragma region RunTime
////protected:
////	void SetSessionId(const uint8_t* sessionId)
////	{
////		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_ID_SIZE; i++)
////		{
////			SessionId[i] = sessionId[i];
////		}
////	}
////
////	void SetRandomSessionId()
////	{
////		RandomSource.GetRandomStreamCrypto(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
////	}
////
////	const bool SessionIdMatches(const uint8_t* sessionId)
////	{
////		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_ID_SIZE; i++)
////		{
////			if (sessionId[i] != SessionId[i])
////			{
////				return false;
////			}
////		}
////		return true;
////	}
////
////	void CopySessionIdTo(uint8_t* target)
////	{
////		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_ID_SIZE; i++)
////		{
////			target[i] = SessionId[i];
////		}
////	}
////
////	/// <summary>
////	/// 
////	/// </summary>
////	/// <param name="key">sizeof = LoLaCryptoDefinition::CYPHER_KEY_SIZE</param>
////	void CalculateExpandedKey1(const uint8_t* key)
////	{
////		// Populate crypto keys with HKDF from secret key.
////		KeyExpander.setKey((uint8_t*)(key), LoLaCryptoDefinition::CYPHER_KEY_SIZE, SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
////	}
////
////	void CalculateExpandedKey2()
////	{
////		// Populate crypto keys with HKDF from secret key.
////		KeyExpander.extract(((uint8_t*)(&ExpandedKey)), LoLaLinkDefinition::HKDFSize);
////
////		// Clear hasher from sensitive material. Disabled for performance.
////		//KeyExpander.clear();
////	}
////
////	/// <summary>
////	/// </summary>
////	/// <param name="localPublicKey"></param>
////	/// <param name="partnerPublicKey"></param>
////	void CalculateSessionAddressing(const uint8_t* localPublicKey, const uint8_t* partnerPublicKey)
////	{
////		KeyHasher.reset(LoLaLinkDefinition::ADDRESS_KEY_SIZE);
////		KeyHasher.update(ExpandedKey.AdressingSeed, LoLaLinkDefinition::ADDRESS_SEED_SIZE);
////		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
////		KeyHasher.update(localPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
////		KeyHasher.finalize(InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
////
////		KeyHasher.reset(LoLaLinkDefinition::ADDRESS_KEY_SIZE);
////		KeyHasher.update(ExpandedKey.AdressingSeed, LoLaLinkDefinition::ADDRESS_SEED_SIZE);
////		KeyHasher.update(localPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
////		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
////		KeyHasher.finalize(OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
////
////		// Set tag zero values, above the tag size.
////		for (uint_fast8_t i = LoLaCryptoDefinition::CYPHER_TAG_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
////		{
////			AuthTag[i] = 0;
////		}
////		// Clear hasher from sensitive material. Disabled for performance.
////		//KeyHasher.clear();
////	}
////
////	void CalculateSessionToken(const uint8_t* partnerPublicKey)
////	{
////		KeyHasher.reset(LoLaLinkDefinition::SESSION_TOKEN_SIZE);
////		KeyHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
////		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
////		KeyHasher.finalize(SessionToken, LoLaLinkDefinition::SESSION_TOKEN_SIZE);
////
////		// Clear hasher from sensitive material. Disabled for performance.
////		//KeyHasher.clear();
////	}
////
////	/// <summary>
////	/// Assumes SessionId has been set.
////	/// </summary>
////	/// <param name="partnerPublicKey"></param>
////	/// <returns>True if the calculated secrets are already set for this SessionId and PublicKey</returns>
////	const bool CachedSessionTokenMatches(const uint8_t* partnerPublicKey)
////	{
////		KeyHasher.reset(LoLaLinkDefinition::SESSION_TOKEN_SIZE);
////		KeyHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
////		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
////		KeyHasher.finalize(MatchToken, LoLaLinkDefinition::SESSION_TOKEN_SIZE);
////
////		// Clear hasher from sensitive material. Disabled for performance.
////		//KeyHasher.clear();
////
////		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_TOKEN_SIZE; i++)
////		{
////			if (MatchToken[i] != SessionToken[i])
////			{
////				return false;
////			}
////		}
////
////		return true;
////	}
////
////	void CopyLinkingTokenTo(uint8_t* target)
////	{
////		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
////		{
////			target[i] = ExpandedKey.LinkingToken[i];
////		}
////	}
////
////	const bool LinkingTokenMatches(const uint8_t* linkingToken)
////	{
////		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
////		{
////			if (linkingToken[i] != ExpandedKey.LinkingToken[i])
////			{
////				return false;
////			}
////		}
////		return true;
////	}
////
////	void GetChallengeSignature(const uint8_t* challenge, const uint8_t* password, uint8_t* signatureTarget)
////	{
////		KeyHasher.reset(LoLaLinkDefinition::CHALLENGE_SIGNATURE_SIZE);
////		KeyHasher.update(challenge, LoLaLinkDefinition::CHALLENGE_CODE_SIZE);
////		KeyHasher.update(password, LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE);
////
////		KeyHasher.finalize(signatureTarget, LoLaLinkDefinition::CHALLENGE_SIGNATURE_SIZE);
////	}
////#pragma endregion
////
////#pragma region Packet
////protected:
////	void EncodeOutPacket(const uint8_t* data, const uint8_t counter, const uint8_t dataSize)
////	{
////		// Plaintext copy of data to output.
////		for (uint_fast8_t i = 0; i < dataSize; i++)
////		{
////			RawOutPacket[i + LoLaPacketDefinition::DATA_INDEX] = data[i];
////		}
////
////		// Set rolling plaintext rolling counter id.
////		RawOutPacket[LoLaPacketDefinition::ID_INDEX] = counter;
////
////		// Set HMAC without implicit addressing or token.
////		CryptoHasher.reset(EmptyKey);
////		CryptoHasher.update(&RawOutPacket[LoLaPacketDefinition::CONTENT_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));
////		CryptoHasher.finalize(EmptyKey, &RawOutPacket[LoLaPacketDefinition::MAC_INDEX], LoLaPacketDefinition::MAC_SIZE);
////	}
////
////	/// <summary>
////	/// Encodes the data to RawOutPacket.
////	/// </summary>
////	/// <param name="data"></param>
////	/// <param name="timestampSeconds"></param>
////	/// <param name="tokenRoll"></param>
////	/// <param name="counter"></param>
////	/// <param name="dataSize"></param>
////	void EncodeOutPacket(const uint8_t* data, const uint32_t timestampSeconds, const uint16_t tokenRoll, const uint8_t counter, const uint8_t dataSize)
////	{
////		//EncodeOutPacket(data, counter, dataSize);	return;
////
////		/*****************/
////		// Set packet authentication tag.
////		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = counter;
////		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_SIZE_INDEX] = dataSize;
////		if (LoLaCryptoDefinition::TIME_NONCE_ENABLED)
////		{
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = (uint8_t)(tokenRoll);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = (uint8_t)(tokenRoll >> 8);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = (uint8_t)(timestampSeconds);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = (uint8_t)(timestampSeconds >> 8);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = (uint8_t)(timestampSeconds >> 16);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = (uint8_t)(timestampSeconds >> 24);
////		}
////		else
////		{
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = 0;
////		}
////		/*****************/
////
////		/*****************/
////		// Reset entropy.
////		CryptoCypher.setKey(ExpandedKey.CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);
////		CryptoCypher.setIV(ExpandedKey.CypherIv, LoLaCryptoDefinition::CYPHER_IV_SIZE);
////
////		// Set cypher counter with mixed cypher counter for packet data.
////		CryptoCypher.setCounter(AuthTag, LoLaCryptoDefinition::CYPHER_TAG_SIZE);
////
////		// Encrypt the everything but the packet id.
////		CryptoCypher.encrypt(&RawOutPacket[LoLaPacketDefinition::DATA_INDEX],
////			data,
////			dataSize);
////
////		// Encrypt the counter.
////		RawOutPacket[LoLaPacketDefinition::ID_INDEX] = ExpandedKey.IdKey[0] ^ AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX];
////
////		// Clear cypher from sensitive material. Disabled for performance.
////		// CryptoCypher.clear();
////		/*****************/
////
////		/*****************/
////		// Start HMAC, now that we have a nonce.
////		CryptoHasher.reset(ExpandedKey.MacKey);
////
////		// Add upper nonce to hash content, as it is discarded on finalization truncation.
////		CryptoHasher.update(&AuthTag[LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE], LoLaCryptoDefinition::CYPHER_TAG_SIZE - LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE);
////
////		// Content.
////		CryptoHasher.update(&RawOutPacket[LoLaPacketDefinition::CONTENT_INDEX],
////			LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));
////
////		// Implicit addressing.
////		CryptoHasher.update(OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
////
////		// HMAC finalized with short nonce and copied to packet.
////		// Only the first LoLaPacketDefinition:MAC_SIZE bytes are effectively used.
////		CryptoHasher.finalize(AuthTag, &RawOutPacket[LoLaPacketDefinition::MAC_INDEX], LoLaPacketDefinition::MAC_SIZE);
////
////		// Clear hasher from sensitive material. Disabled for performance.
////		//CryptoHasher.clear();
////		/*****************/
////	}
////
////	/// <summary>
////	/// 
////	/// </summary>
////	/// <param name="counter"></param>
////	/// <param name="dataSize"></param>
////	/// <returns>True if packet was accepted.</returns>
////	const bool DecodeInPacket(uint8_t& counter, const uint8_t dataSize)
////	{
////		// Calculate MAC from content.
////		CryptoHasher.reset(EmptyKey);
////		CryptoHasher.update(&RawInPacket[LoLaPacketDefinition::CONTENT_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));
////		CryptoHasher.finalize(EmptyKey, InputMac, LoLaPacketDefinition::MAC_SIZE);
////
////		// Reject if HMAC mismatches plaintext MAC from packet.
////		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAC_SIZE; i++)
////		{
////			if (InputMac[i] != RawInPacket[LoLaPacketDefinition::MAC_INDEX + i])
////			{
////				// Packet rejected.
////				counter = 0;
////				return false;
////			}
////		}
////
////		// Copy plaintext counter.
////		counter = RawInPacket[LoLaPacketDefinition::ID_INDEX];
////
////		// Copy plaintext content to in data.
////		for (uint_fast8_t i = 0; i < dataSize; i++)
////		{
////			InData[i] = RawInPacket[i + LoLaPacketDefinition::DATA_INDEX];
////		}
////
////		return true;
////	}
////
////	/// <summary>
////	/// 
////	/// </summary>
////	/// <param name="counter"></param>
////	/// <param name="timestampSeconds"></param>
////	/// <param name="tokenRoll"></param>
////	/// <param name="dataSize"></param>
////	/// <returns>True if packet was accepted.</returns>
////	const bool DecodeInPacket(uint8_t& counter, const uint32_t timestampSeconds, const uint16_t tokenRoll, const uint8_t dataSize)
////	{
////		//return DecodeInPacket(counter, dataSize);
////
////		/*****************/
////		// Set packet authentication tag.
////		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX] = ExpandedKey.IdKey[0] ^ RawInPacket[LoLaPacketDefinition::ID_INDEX];
////		AuthTag[LoLaCryptoDefinition::CYPHER_TAG_SIZE_INDEX] = dataSize;
////		if (LoLaCryptoDefinition::TIME_NONCE_ENABLED)
////		{
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = (uint8_t)(tokenRoll);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = (uint8_t)(tokenRoll >> 8);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = (uint8_t)(timestampSeconds);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = (uint8_t)(timestampSeconds >> 8);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = (uint8_t)(timestampSeconds >> 16);
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = (uint8_t)(timestampSeconds >> 24);
////		}
////		else
////		{
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ROLL_INDEX + 1] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 1] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 2] = 0;
////			AuthTag[LoLaCryptoDefinition::CYPHER_TAG_TIMESTAMP_INDEX + 3] = 0;
////		}
////		/*****************/
////
////		/*****************/
////		// Start HMAC with 16 byte Auth Key.
////		CryptoHasher.reset(ExpandedKey.MacKey);
////
////		// Add upper nonce to hash content, as it is discarded on finalization truncation.
////		CryptoHasher.update(&AuthTag[LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE], LoLaCryptoDefinition::CYPHER_TAG_SIZE - LoLaCryptoDefinition::NONCE_EFFECTIVE_SIZE);
////
////		// Content.
////		CryptoHasher.update(&RawInPacket[LoLaPacketDefinition::CONTENT_INDEX], LoLaPacketDefinition::GetContentSizeFromDataSize(dataSize));
////
////		// Implicit addressing. [Receiver|Sender]
////		CryptoHasher.update(InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
////
////		// HMAC finalized with short nonce and copied to InputMac.
////		// Only the first MAC_SIZE bytes are effectively used.
////		CryptoHasher.finalize(AuthTag, InputMac, LoLaPacketDefinition::MAC_SIZE);
////
////		// Clear hasher from sensitive material. Disabled for performance.
////		//CryptoHasher.clear();
////		/*****************/
////
////		/*****************/
////		// Reject if plaintext MAC from packet mismatches.
////		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAC_SIZE; i++)
////		{
////			// Packet rejected.
////			if (InputMac[i] != RawInPacket[LoLaPacketDefinition::MAC_INDEX + i])
////			{
////				counter = 0;
////				return false;
////			}
////		}
////		/*****************/
////
////		/*****************/
////		// Reset entropy.
////		CryptoCypher.setKey(ExpandedKey.CypherKey, LoLaCryptoDefinition::CYPHER_KEY_SIZE);
////		CryptoCypher.setIV(ExpandedKey.CypherIv, LoLaCryptoDefinition::CYPHER_IV_SIZE);
////
////		// Set the cypher counter with the AuthTag.
////		CryptoCypher.setCounter(AuthTag, LoLaCryptoDefinition::CYPHER_TAG_SIZE);
////
////		// Decrypt everything but the id.
////		CryptoCypher.decrypt(InData,
////			&RawInPacket[LoLaPacketDefinition::DATA_INDEX],
////			dataSize);
////
////		// Get counter from packet id.
////		counter = AuthTag[LoLaCryptoDefinition::CYPHER_TAG_ID_INDEX];
////
////		// Clear cypher from sensitive material. Disabled for performance.
////		//CryptoCypher.clear();
////		/*****************/
////
////		return true;
////	}
////#pragma endregion
////};
//
////#pragma region RunTime
//	//protected:
//	//	void SetSessionId(const uint8_t* sessionId)
//	//	{
//	//		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_ID_SIZE; i++)
//	//		{
//	//			SessionId[i] = sessionId[i];
//	//		}
//	//	}
//	//
//	//	void SetRandomSessionId()
//	//	{
//	//		RandomSource.GetRandomStreamCrypto(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
//	//	}
//	//
//	//	const bool SessionIdMatches(const uint8_t* sessionId)
//	//	{
//	//		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_ID_SIZE; i++)
//	//		{
//	//			if (sessionId[i] != SessionId[i])
//	//			{
//	//				return false;
//	//			}
//	//		}
//	//		return true;
//	//	}
//	//
//	//	void CopySessionIdTo(uint8_t* target)
//	//	{
//	//		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_ID_SIZE; i++)
//	//		{
//	//			target[i] = SessionId[i];
//	//		}
//	//	}
//	//
//	//	/// <summary>
//	//	/// 
//	//	/// </summary>
//	//	/// <param name="key">sizeof = LoLaCryptoDefinition::CYPHER_KEY_SIZE</param>
//	//	void CalculateExpandedKey1(const uint8_t* key)
//	//	{
//	//		// Populate crypto keys with HKDF from secret key.
//	//		KeyExpander.setKey((uint8_t*)(key), LoLaCryptoDefinition::CYPHER_KEY_SIZE, SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
//	//	}
//	//
//	//	void CalculateExpandedKey2()
//	//	{
//	//		// Populate crypto keys with HKDF from secret key.
//	//		KeyExpander.extract(((uint8_t*)(&ExpandedKey)), LoLaLinkDefinition::HKDFSize);
//	//
//	//		// Clear hasher from sensitive material. Disabled for performance.
//	//		//KeyExpander.clear();
//	//	}
//	//
//	//	/// <summary>
//	//	/// </summary>
//	//	/// <param name="localPublicKey"></param>
//	//	/// <param name="partnerPublicKey"></param>
//	//	void CalculateSessionAddressing(const uint8_t* localPublicKey, const uint8_t* partnerPublicKey)
//	//	{
//	//		KeyHasher.reset(LoLaLinkDefinition::ADDRESS_KEY_SIZE);
//	//		KeyHasher.update(ExpandedKey.AdressingSeed, LoLaLinkDefinition::ADDRESS_SEED_SIZE);
//	//		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
//	//		KeyHasher.update(localPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
//	//		KeyHasher.finalize(InputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
//	//
//	//		KeyHasher.reset(LoLaLinkDefinition::ADDRESS_KEY_SIZE);
//	//		KeyHasher.update(ExpandedKey.AdressingSeed, LoLaLinkDefinition::ADDRESS_SEED_SIZE);
//	//		KeyHasher.update(localPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
//	//		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
//	//		KeyHasher.finalize(OutputKey, LoLaLinkDefinition::ADDRESS_KEY_SIZE);
//	//
//	//		// Set tag zero values, above the tag size.
//	//		for (uint_fast8_t i = LoLaCryptoDefinition::CYPHER_TAG_SIZE; i < LoLaCryptoDefinition::MAC_KEY_SIZE; i++)
//	//		{
//	//			AuthTag[i] = 0;
//	//		}
//	//		// Clear hasher from sensitive material. Disabled for performance.
//	//		//KeyHasher.clear();
//	//	}
//	//
//	//	void CalculateSessionToken(const uint8_t* partnerPublicKey)
//	//	{
//	//		KeyHasher.reset(LoLaLinkDefinition::SESSION_TOKEN_SIZE);
//	//		KeyHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
//	//		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
//	//		KeyHasher.finalize(SessionToken, LoLaLinkDefinition::SESSION_TOKEN_SIZE);
//	//
//	//		// Clear hasher from sensitive material. Disabled for performance.
//	//		//KeyHasher.clear();
//	//	}
//	//
//	//	/// <summary>
//	//	/// Assumes SessionId has been set.
//	//	/// </summary>
//	//	/// <param name="partnerPublicKey"></param>
//	//	/// <returns>True if the calculated secrets are already set for this SessionId and PublicKey</returns>
//	//	const bool CachedSessionTokenMatches(const uint8_t* partnerPublicKey)
//	//	{
//	//		KeyHasher.reset(LoLaLinkDefinition::SESSION_TOKEN_SIZE);
//	//		KeyHasher.update(SessionId, LoLaLinkDefinition::SESSION_ID_SIZE);
//	//		KeyHasher.update(partnerPublicKey, LoLaCryptoDefinition::PUBLIC_KEY_SIZE);
//	//		KeyHasher.finalize(MatchToken, LoLaLinkDefinition::SESSION_TOKEN_SIZE);
//	//
//	//		// Clear hasher from sensitive material. Disabled for performance.
//	//		//KeyHasher.clear();
//	//
//	//		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::SESSION_TOKEN_SIZE; i++)
//	//		{
//	//			if (MatchToken[i] != SessionToken[i])
//	//			{
//	//				return false;
//	//			}
//	//		}
//	//
//	//		return true;
//	//	}
//	//
//	//	void CopyLinkingTokenTo(uint8_t* target)
//	//	{
//	//		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
//	//		{
//	//			target[i] = ExpandedKey.LinkingToken[i];
//	//		}
//	//	}
//	//
//	//	const bool LinkingTokenMatches(const uint8_t* linkingToken)
//	//	{
//	//		for (uint_fast8_t i = 0; i < LoLaLinkDefinition::LINKING_TOKEN_SIZE; i++)
//	//		{
//	//			if (linkingToken[i] != ExpandedKey.LinkingToken[i])
//	//			{
//	//				return false;
//	//			}
//	//		}
//	//		return true;
//	//	}
//	//
//	//	void GetChallengeSignature(const uint8_t* challenge, const uint8_t* password, uint8_t* signatureTarget)
//	//	{
//	//		KeyHasher.reset(LoLaLinkDefinition::CHALLENGE_SIGNATURE_SIZE);
//	//		KeyHasher.update(challenge, LoLaLinkDefinition::CHALLENGE_CODE_SIZE);
//	//		KeyHasher.update(password, LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE);
//	//
//	//		KeyHasher.finalize(signatureTarget, LoLaLinkDefinition::CHALLENGE_SIGNATURE_SIZE);
//	//	}
//	//#pragma endregion
//#endif