// LoLaLinkDefinition.h

#ifndef _LOLA_LINK_DEFINITION_h
#define _LOLA_LINK_DEFINITION_h

#include "Crypto\LoLaCryptoDefinition.h"
#include "LoLaPacketDefinition.h"

/// <summary>
/// Link uses 3 registered ports:
/// - Unencrypted port for link start or quick re-link.
/// - Encrypted port. Tokens may change during linking.
/// - Link-time service port. Encrypted with token, as are all user services.
/// </summary>
class LoLaLinkDefinition
{
public:
	// <summary>
	/// When linked, top port is reserved.
	/// When unlinked, only top 2 ports are used.
	/// </summary>
	static constexpr uint8_t LINK_PORT_ALLOCATION = 1;
	static constexpr uint8_t MAX_DEFINITION_PORT = UINT8_MAX - LINK_PORT_ALLOCATION;

	/// <summary>
	/// 32 bit session id.
	/// </summary>
	static constexpr uint8_t SESSION_ID_SIZE = 4;

	/// <summary>
	/// Session linking token.
	/// </summary>
	static constexpr uint8_t SESSION_TOKEN_SIZE = 4;

	/// <summary>
	/// Implicit addressing key size.
	/// </summary>
	static constexpr uint8_t ADDRESS_KEY_SIZE = 4;

	/// <summary>
	/// Implicit addressing entropy size.
	/// </summary>
	static constexpr uint8_t ADDRESS_SEED_SIZE = 4;

	/// <summary>
	/// Channel PRNG entropy/key size.
	/// </summary>
	static constexpr uint8_t CHANNEL_KEY_SIZE = 4;

	/// <summary>
	/// One time linking token.
	/// Hash mix of SessionId and Public Key pairs.
	/// </summary>
	static constexpr uint8_t LINKING_TOKEN_SIZE = 4;

	/// <summary>
	/// A static password that's required to access a device.
	/// </summary>
	static constexpr uint8_t ACCESS_CONTROL_PASSWORD_SIZE = 8;


	/// <summary>
	/// Timestamps are in 32 bit UTC seconds or microseconds.
	/// </summary>
	static constexpr uint8_t TIME_SIZE = 4;

public:
	/// <summary>
	/// Period of sub-token update.
	/// 20 ms guarantees up to 12800 packets/second without nonce re-use.
	/// </summary>
	static constexpr uint32_t SUB_TOKEN_PERIOD_MICROS = 20000;

public:
	/// <summary>
	/// The raw keys that are used during runtime encryption.
	/// These are filled in with HKDF from the shared secret key.
	/// </summary>
	struct ExpandedKeyStruct
	{
		/// <summary>
		/// Cypher Key and IV.
		/// </summary>
		uint8_t CypherKey[LoLaCryptoDefinition::CYPHER_KEY_SIZE];
		uint8_t CypherIv[LoLaCryptoDefinition::CYPHER_IV_SIZE];

		/// <summary>
		/// MAC Key.
		/// </summary>
		uint8_t MacKey[LoLaCryptoDefinition::MAC_KEY_SIZE];

		/// <summary>
		/// Packet Id Key.
		/// </summary>
		uint8_t IdKey[LoLaCryptoDefinition::CYPHER_TAG_ID_SIZE];

		/// <summary>
		/// Addressing entropy.
		/// </summary>
		uint8_t AdressingSeed[ADDRESS_SEED_SIZE];

		/// <summary>
		/// Ephemeral session linking start token.
		/// </summary>
		uint8_t LinkingToken[LINKING_TOKEN_SIZE];

		/// <summary>
		/// Channel PRNG seed.
		/// </summary>
		uint8_t ChannelSeed[CHANNEL_KEY_SIZE];
	};

	static constexpr uint8_t HKDFSize = sizeof(ExpandedKeyStruct);

public:
	/// <summary>
	/// 3 counts: +1 for sender sending report, +1 for local offset, +1 for margin.
	/// </summary>
	static constexpr uint8_t ROLLING_COUNTER_TOLERANCE = 3;

	/// <summary>
	/// Link will tolerate up to ~20% dropped packets without any correction.
	/// </summary>
	static constexpr uint8_t ROLLING_COUNTER_ERROR = UINT8_MAX / 5;

	/// <summary>
	/// Link switch over will tolerate +-0.5% error (us) at the start.
	/// During link time, clock will be continously tuned for higher accuracy.
	/// </summary>
	static constexpr int32_t LINKING_CLOCK_TOLERANCE = 50;

	/// <summary>
	/// </summary>
	static constexpr int32_t LINKING_TRANSITION_PERIOD_MICROS = 40000;

	/// <summary>
	/// If linking is not complete after this time, unlink and restart.
	/// </summary>
	static constexpr uint32_t LINKING_STAGE_TIMEOUT = 333;

	/// <summary>
	/// How long without an input message from partner before disconnect.
	/// </summary>
	static constexpr uint32_t LINK_STAGE_TIMEOUT = 1500;

	/// <summary>
	/// How long to wait before timing out a send.
	/// </summary>
	static constexpr uint32_t TRANSMIT_BASE_TIMEOUT_MICROS = 3100;

	/// <summary>
	/// 
	/// </summary>
	static constexpr uint32_t PRE_LINK_DUPLEX_MICROS = 6000;


	/// <summary>
	/// How long to wait before re-sending an answered packet.
	/// </summary>
	static constexpr uint32_t RE_TRANSMIT_TIMEOUT_MICROS = 4000;

	/// <summary>
	/// Report target update rate. Slow value, let the main services hog the link.
	/// </summary>
	static constexpr uint32_t REPORT_UPDATE_PERIOD = 250;

	/// <summary>
	/// If no other service is getting messages, how long to trigger a report back.
	/// </summary>
	static constexpr uint32_t REPORT_PARTNER_SILENCE_TRIGGER_PERIOD = 150;

	/// <summary>
	/// Report (average) send back off period.
	/// </summary>
	static constexpr uint8_t REPORT_RESEND_PERIOD = 40;

	/// <summary>
	/// How many microseconds is the client allowed to tune its clock on one tune request. 
	/// </summary>
	static constexpr uint32_t CLOCK_TUNE_RANGE_MICROS = 300;

	static constexpr uint32_t CLOCK_TUNE_PERIOD = 666;
	static constexpr uint32_t CLOCK_TUNE_RETRY_PERIOD = 33;

private:
	/// <summary>
	/// ||SessionId|CompressedPublicKey||
	/// </summary>
	template<const uint8_t Header>
	struct PkeBroadcastDefinition : public TemplateHeaderDefinition<Header, SESSION_ID_SIZE + LoLaCryptoDefinition::COMPRESSED_KEY_SIZE>
	{
		static constexpr uint8_t PAYLOAD_SESSION_ID_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_PUBLIC_KEY_INDEX = PAYLOAD_SESSION_ID_INDEX + SESSION_ID_SIZE;
	};

	/// <summary>
	/// ||SessionToken||
	/// </summary>
	template<const uint8_t Header, const uint8_t ExtraSize>
	struct LinkingSwitchOverDefinition : public TemplateHeaderDefinition<Header, LINKING_TOKEN_SIZE + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_SESSION_TOKEN_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
	};


	template<const uint8_t Header, const uint8_t ExtraSize = 0>
	struct ClockTimestampDefinition : public TemplateHeaderDefinition<Header, (TIME_SIZE * 2) + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_SECONDS_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_SUB_SECONDS_INDEX = PAYLOAD_SECONDS_INDEX + TIME_SIZE;
	};

	template<const uint8_t Header, const uint8_t ExtraSize = 0>
	struct LinkSwitchOverDefinition : public TemplateHeaderDefinition<Header, LINKING_TOKEN_SIZE + ExtraSize>
	{};

public:
	/// <summary>
	/// Abstract struct just to expose generic size and payload indexes of time messages.
	/// </summary>
	struct Unlinked
	{
	public:
		static constexpr uint8_t PORT = UINT8_MAX;

		/// <summary>
		/// ||||
		/// Broadcast to search for available partners.
		/// TODO: Add support for search for specific Host Id or Device category.
		/// </summary>
		using SearchRequest = TemplateHeaderDefinition<0, 0>;

		/// <summary>
		/// Quick reply to let partner know where we're here.
		/// Used in channel searching before starting a session.
		/// </summary>
		using SearchReply = TemplateHeaderDefinition<SearchRequest::HEADER + 1, 0>;

		/// <summary>
		/// ||||
		/// Request server to start a PKE session.
		/// TODO: Add support for search for specific Device Id.
		/// </summary>
		using SessionRequest = TemplateHeaderDefinition<SearchReply::HEADER + 1, 0>;

		/// <summary>
		/// ||SessionId|CompressedServerPublicKey||
		/// </summary>
		using SessionAvailable = PkeBroadcastDefinition<SessionRequest::HEADER + 1>;

		/// <summary>
		/// ||SessionId|CompressedClientPublicKey||
		/// </summary>
		using LinkingStartRequest = PkeBroadcastDefinition<SessionAvailable::HEADER + 1>;

		/// <summary>
		/// ||SessionToken|Remaining||
		/// </summary>
		struct LinkingTimedSwitchOver : public LinkingSwitchOverDefinition<LinkingStartRequest::HEADER + 1, TIME_SIZE>
		{
			using BaseClass = LinkingSwitchOverDefinition<LinkingStartRequest::HEADER + 1, TIME_SIZE>;

			static constexpr uint8_t PAYLOAD_TIME_INDEX = BaseClass::PAYLOAD_SESSION_TOKEN_INDEX + LINKING_TOKEN_SIZE;
		};

		/// <summary>
		/// ||SessionToken||
		/// </summary>
		using LinkingTimedSwitchOverAck = LinkingSwitchOverDefinition<LinkingTimedSwitchOver::HEADER + 1, 0>;
	};

	struct Linking
	{
		static constexpr uint8_t PORT = Unlinked::PORT - 1;

		/// <summary>
		/// ||ChallengeCode||
		/// </summary>
		//using LinkChallenge = TemplateHeaderDefinition<0, CHALLENGE_REQUEST_SIZE>;
		struct ServerChallengeRequest : public TemplateHeaderDefinition<0, LoLaCryptoDefinition::CHALLENGE_CODE_SIZE>
		{
			static constexpr uint8_t PAYLOAD_CHALLENGE_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		};

		/// <summary>
		/// ||SignedCode|ChallengeCode||
		/// </summary>
		struct ClientChallengeReplyRequest : public TemplateHeaderDefinition<ServerChallengeRequest::HEADER + 1, LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE + LoLaCryptoDefinition::CHALLENGE_CODE_SIZE>
		{
			static constexpr uint8_t PAYLOAD_SIGNED_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
			static constexpr uint8_t PAYLOAD_CHALLENGE_INDEX = PAYLOAD_SIGNED_INDEX + LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE;
		};

		/// <summary>
		/// ||SignedCode||
		/// </summary>
		struct ServerChallengeReply : public TemplateHeaderDefinition<ClientChallengeReplyRequest::HEADER + 1, LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE>
		{
			static constexpr uint8_t PAYLOAD_SIGNED_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		};

		/// <summary>
		/// ||Seconds|SubSeconds|||
		/// Seconds in full UTC seconds.
		/// Sub-seconds in microseconds [-999999; +999999]
		/// </summary>
		using ClockSyncRequest = ClockTimestampDefinition<ServerChallengeReply::HEADER + 1>;

		/// <summary>
		/// ||Seconds|SubSecondsError|Accepted||
		/// Sub-seconds in microseconds [-999999; +999999]
		/// Accepted true if non-zero.
		/// </summary>
		struct ClockSyncReply : public ClockTimestampDefinition<ClockSyncRequest::HEADER + 1, 1>
		{
			using BaseClass = ClockTimestampDefinition<ClockSyncRequest::HEADER + 1, 1>;

			static constexpr uint8_t PAYLOAD_ACCEPTED_INDEX = BaseClass::PAYLOAD_SUB_SECONDS_INDEX + TIME_SIZE;
		};

		/// <summary>
		/// 
		/// </summary>
		using StartLinkRequest = TemplateHeaderDefinition<ClockSyncReply::HEADER + 1, 0>;

		/// <summary>
		/// ||Remaining||
		/// </summary>
		struct LinkTimedSwitchOver : public LinkSwitchOverDefinition<StartLinkRequest::HEADER + 1, TIME_SIZE>
		{
			static constexpr uint8_t PAYLOAD_TIME_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		};

		/// <summary>
		/// ||||
		/// </summary>
		using LinkTimedSwitchOverAck = LinkSwitchOverDefinition<LinkTimedSwitchOver::HEADER + 1>;
	};

	struct Linked
	{
	public:
		/// <summary>
		/// </summary>
		static constexpr uint8_t PORT = UINT8_MAX;

		/// <summary>
		/// ||Average RSSI|ReceiveCounter|REQUEST_REPLY||
		/// </summary>
		struct ReportUpdate : public TemplateHeaderDefinition<0, 3>
		{
			static constexpr uint8_t PAYLOAD_RSSI_INDEX = SUB_PAYLOAD_INDEX;
			static constexpr uint8_t PAYLOAD_RECEIVE_COUNTER_INDEX = PAYLOAD_RSSI_INDEX + 1;
			static constexpr uint8_t PAYLOAD_REQUEST_INDEX = PAYLOAD_RECEIVE_COUNTER_INDEX + 1;
		};

		/// <summary>
		/// ||Rolling Time (us)||
		/// </summary>
		struct ClockTuneMicrosRequest : public TemplateHeaderDefinition<ReportUpdate::HEADER + 1, TIME_SIZE>
		{
			static constexpr uint8_t PAYLOAD_ROLLING_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		};

		/// <summary>
		/// ||Rolling Time error (us)||
		/// </summary>
		struct ClockTuneMicrosReply : public TemplateHeaderDefinition<ClockTuneMicrosRequest::HEADER + 1, TIME_SIZE>
		{
			static constexpr uint8_t PAYLOAD_ERROR_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		};
	};

	/// <summary>
	/// Largest Link Packets will be the Broadcast packets, that contain the SessionId+PublicKey(compressed).
	/// </summary>
	static constexpr uint8_t LARGEST_PAYLOAD = PkeBroadcastDefinition<0>::PAYLOAD_SIZE;
};
#endif