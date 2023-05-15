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
	/// When unlinked, top 2 ports are reserved.
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
	/// Follows the size of the PRNG hash.
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
	/// Link switch over will tolerate some error (us) at the start.
	/// During link time, clock will be continously tuned for higher accuracy.
	/// </summary>
	static constexpr int32_t LINKING_CLOCK_TOLERANCE = 60;

	/// <summary>
	/// </summary>
	static constexpr int32_t LINKING_TRANSITION_PERIOD_MICROS = 30000;
	static constexpr int32_t LINKING_TRANSITION_RESEND_PERIOD_MICROS = 6666;

	/// <summary>
	/// If linking is not complete after this time, unlink and restart.
	/// </summary>
	static constexpr uint32_t LINKING_STAGE_TIMEOUT = 200;
	/// <summary>
	/// How long without an input message from partner before disconnect.
	/// </summary>
	static constexpr uint32_t LINK_STAGE_TIMEOUT = 1500;

	/// <summary>
	/// How long to wait before timing out a send.
	/// </summary>
	static constexpr uint32_t TRANSMIT_BASE_TIMEOUT_MICROS = 2000;

	/// <summary>
	/// How long to wait before timing out an ACK reply.
	/// </summary>
	static constexpr uint32_t REPLY_BASE_TIMEOUT_MICROS = (TRANSMIT_BASE_TIMEOUT_MICROS * 2) + 5000;

	/// <summary>
	/// Report target update rate. Slow value, let the main services hog the link.
	/// </summary>
	static constexpr uint32_t REPORT_UPDATE_PERIOD = 333;

	/// <summary>
	/// If no other service is getting messages, how long to trigger a report back.
	/// </summary>
	static constexpr uint32_t REPORT_PARTNER_SILENCE_TRIGGER_PERIOD = 100;

	/// <summary>
	/// Report (average) send back off period.
	/// </summary>
	static constexpr uint8_t REPORT_RESEND_PERIOD = 40;

private:
	template<const uint8_t SubHeader>
	struct BroadcastDefinition : public TemplateSubHeaderDefinition<SubHeader, SESSION_ID_SIZE + LoLaCryptoDefinition::COMPRESSED_KEY_SIZE>
	{
		/// <summary>
		/// ||SessionId|PublicKey||
		/// </summary>
		static constexpr uint8_t PAYLOAD_SESSION_ID_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_PUBLIC_KEY_INDEX = PAYLOAD_SESSION_ID_INDEX + SESSION_ID_SIZE;
	};

	template<const uint8_t SubHeader, const uint8_t ExtraSize>
	struct BroadcastSwitchOverDefinition : public TemplateSubHeaderDefinition<SubHeader, LINKING_TOKEN_SIZE + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_SESSION_TOKEN_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
	};

	/// <summary>
	/// Clock Sync request have the same format.
	/// </summary>
	template<const uint8_t SubHeader>
	struct ClockSyncDefinition : public TemplateSubHeaderDefinition<SubHeader, TIME_SIZE>
	{
		static constexpr uint8_t PAYLOAD_TIME_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
	};

	template<const uint8_t SubHeader, const uint8_t ExtraSize = 0>
	struct ClockTimestampDefinition : public TemplateSubHeaderDefinition<SubHeader, (TIME_SIZE * 2) + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_SECONDS_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_SUB_SECONDS_INDEX = PAYLOAD_SECONDS_INDEX + TIME_SIZE;
	};

	template<const uint8_t SubHeader, const uint8_t ExtraSize = 0>
	struct LinkingSwitchOverDefinition : public TemplateSubHeaderDefinition<SubHeader, LINKING_TOKEN_SIZE + ExtraSize>
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
		/// ||Partner Acknowledged.||
		/// Has received a previous discovery from the sender.
		/// </summary>
		struct PartnerDiscovery : public TemplateSubHeaderDefinition<0, 1>
		{
			static constexpr uint8_t PAYLOAD_PARTNER_ACKNOWLEDGED_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
		};

		/// <summary>
		/// Broadcast to search for available partners.
		/// TODO: Add support for search for specific Host Id or Device category.
		/// </summary>
		using SearchRequest = TemplateSubHeaderDefinition<0, 0>;

		/// <summary>
		/// Quick reply to let partner know where we're here.
		/// Used in channel searching before starting a session.
		/// </summary>
		using SearchReply = TemplateSubHeaderDefinition<SearchRequest::SUB_HEADER + 1, 0>;

		/// <summary>
		/// Request host to start a session.
		/// TODO: Add support for search for specific Host Id.
		/// </summary>
		using SessionRequest = TemplateSubHeaderDefinition<SearchReply::SUB_HEADER + 1, 0>;

		/// <summary>
		/// ||SessionId|HostPublicKey||
		/// </summary>
		using SessionBroadcast = BroadcastDefinition<SessionRequest::SUB_HEADER + 1>;

		/// <summary>
		/// ||SessionId|RemotePublicKey||
		/// </summary>
		using LinkingStartRequest = BroadcastDefinition<SessionBroadcast::SUB_HEADER + 1>;

		/// <summary>
		/// ||SessionToken|Remaining||
		/// </summary>
		struct LinkingTimedSwitchOver : public BroadcastSwitchOverDefinition<LinkingStartRequest::SUB_HEADER + 1, TIME_SIZE>
		{
			using BaseClass = BroadcastSwitchOverDefinition<LinkingStartRequest::SUB_HEADER + 1, TIME_SIZE>;

			static constexpr uint8_t PAYLOAD_TIME_INDEX = BaseClass::PAYLOAD_SESSION_TOKEN_INDEX + LINKING_TOKEN_SIZE;
		};

		/// <summary>
		/// ||SessionToken||
		/// </summary>
		using LinkingTimedSwitchOverAck = BroadcastSwitchOverDefinition<LinkingTimedSwitchOver::SUB_HEADER + 1, 0>;
	};

	struct Linking
	{
		static constexpr uint8_t PORT = Unlinked::PORT - 1;

		/// <summary>
		/// ||ChallengeCode||
		/// </summary>
		//using LinkChallenge = TemplateSubHeaderDefinition<0, CHALLENGE_REQUEST_SIZE>;
		struct HostChallengeRequest : public TemplateSubHeaderDefinition<0, LoLaCryptoDefinition::CHALLENGE_CODE_SIZE>
		{
			static constexpr uint8_t PAYLOAD_CHALLENGE_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
		};

		/// <summary>
		/// ||SignedCode|ChallengeCode||
		/// </summary>
		struct RemoteChallengeReplyRequest : public TemplateSubHeaderDefinition<HostChallengeRequest::SUB_HEADER + 1, LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE + LoLaCryptoDefinition::CHALLENGE_CODE_SIZE>
		{
			static constexpr uint8_t PAYLOAD_SIGNED_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
			static constexpr uint8_t PAYLOAD_CHALLENGE_INDEX = PAYLOAD_SIGNED_INDEX + LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE;
		};

		/// <summary>
		/// ||SignedCode||
		/// </summary>
		struct HostChallengeReply : public TemplateSubHeaderDefinition<HostChallengeRequest::SUB_HEADER + 1, LoLaCryptoDefinition::CHALLENGE_SIGNATURE_SIZE>
		{
			static constexpr uint8_t PAYLOAD_SIGNED_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
		};

		/// <summary>
		/// ||Seconds|SubSeconds|||
		/// Seconds in full UTC seconds.
		/// Sub-seconds in microseconds [-999999; +999999]
		/// </summary>
		using ClockSyncRequest = ClockTimestampDefinition<HostChallengeReply::SUB_HEADER + 1>;

		/// <summary>
		/// ||Seconds|SubSecondsError|Accepted||
		/// Sub-seconds in microseconds [-999999; +999999]
		/// Accepted true if non-zero.
		/// </summary>
		struct ClockSyncReply : public ClockTimestampDefinition<ClockSyncRequest::SUB_HEADER + 1, 1>
		{
			using BaseClass = ClockTimestampDefinition<ClockSyncRequest::SUB_HEADER + 1, 1>;

			static constexpr uint8_t PAYLOAD_ACCEPTED_INDEX = BaseClass::PAYLOAD_SUB_SECONDS_INDEX + TIME_SIZE;
		};

		/// <summary>
		/// 
		/// </summary>
		using StartLinkRequest = TemplateSubHeaderDefinition<ClockSyncReply::SUB_HEADER + 1, 0>;

		/// <summary>
		/// ||Remaining||
		/// </summary>
		struct LinkTimedSwitchOver : public LinkingSwitchOverDefinition<StartLinkRequest::SUB_HEADER + 1, TIME_SIZE>
		{
			static constexpr uint8_t PAYLOAD_TIME_INDEX = SubHeaderDefinition::SUB_PAYLOAD_INDEX;
		};

		/// <summary>
		/// ||||
		/// </summary>
		using LinkTimedSwitchOverAck = LinkingSwitchOverDefinition<LinkTimedSwitchOver::SUB_HEADER + 1>;
	};

	struct Linked
	{
	public:
		/// <summary>
		/// Linked and Unlinked share the same port. Maybe not?
		/// </summary>
		static constexpr uint8_t PORT = Linking::PORT - 1;

		/// <summary>
		/// ||Average RSSI|ReceiveCounter|REQUEST_REPLY||
		/// </summary>
		struct ReportUpdate : public TemplateSubHeaderDefinition<0, 3>
		{
			static constexpr uint8_t PAYLOAD_RSSI_INDEX = SUB_PAYLOAD_INDEX;
			static constexpr uint8_t PAYLOAD_RECEIVE_COUNTER_INDEX = PAYLOAD_RSSI_INDEX + 1;
			static constexpr uint8_t PAYLOAD_REQUEST_INDEX = PAYLOAD_RECEIVE_COUNTER_INDEX + 1;
		};

		using ClockTuneMicrosRequest = ClockSyncDefinition<ReportUpdate::SUB_HEADER + 1 >;
		using ClockTuneMicrosReply = ClockSyncDefinition<ClockTuneMicrosRequest::SUB_HEADER + 1>;
	};

	/// <summary>
	/// Largest Link Packets will be the Broadcast packets, that contain the SessionId+PublicKey(compressed).
	/// </summary>
	static constexpr uint8_t LARGEST_PAYLOAD = BroadcastDefinition<0>::PAYLOAD_SIZE;
};
#endif