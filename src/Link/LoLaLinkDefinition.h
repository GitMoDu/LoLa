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
	static constexpr uint8_t LOLA_VERSION = 1;

	// <summary>
	/// When linked, top port is reserved.
	/// When unlinked, only top 2 ports are used.
	/// </summary>
	static constexpr uint8_t LINK_PORT_ALLOCATION = 1;
	static constexpr uint8_t UN_LINKED_PORT_ALLOCATION = 2;
	static constexpr uint8_t MAX_DEFINITION_PORT = UINT8_MAX - LINK_PORT_ALLOCATION;

	/// <summary>
	/// 32 bit session id.
	/// </summary>
	static constexpr uint8_t SESSION_ID_SIZE = 4;

	/// <summary>
	/// 32 bit protocol id.
	/// </summary>
	static constexpr uint8_t PROTOCOL_ID_SIZE = 4;

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
		uint8_t IdKey[LoLaCryptoDefinition::PACKET_ID_SIZE];

		/// <summary>
		/// Addressing entropy.
		/// </summary>
		uint8_t AdressingSeed[ADDRESS_SEED_SIZE];

		///// <summary>
		///// Ephemeral session linking start token.
		///// </summary>
		uint8_t LinkingSeed[LINKING_TOKEN_SIZE];

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
	static constexpr int8_t LINKING_CLOCK_TOLERANCE = 50;

	/// <summary>
	/// </summary>
	static constexpr uint16_t LINKING_TRANSITION_PERIOD_MICROS = 33333;

	/// <summary>
	/// If linking is not complete after this time, unlink and restart.
	/// </summary>
	static constexpr uint16_t LINKING_STAGE_TIMEOUT = 200;

	/// <summary>
	/// How long without an input message from partner before disconnect.
	/// </summary>
	static constexpr uint16_t LINK_STAGE_TIMEOUT = 1500;

	/// <summary>
	/// Duplex periods over this value are too long for LoLa to work effectively.
	/// </summary>
	static constexpr uint16_t DUPLEX_PERIOD_MAX_MICROS = 15000;

	/// <summary>
	/// Extra wait on full duplex to avoid self-collision.
	/// </summary>
	static constexpr uint16_t FULL_DUPLEX_RESEND_WAIT_MICROS = 100;

	/// <summary>
	/// Report target update rate. Slow value, let the main services hog the link.
	/// </summary>
	static constexpr uint16_t REPORT_UPDATE_PERIOD = 250;

	/// <summary>
	/// If no other service is getting messages, how long to trigger a report back.
	/// </summary>
	static constexpr uint8_t REPORT_PARTNER_SILENCE_TRIGGER_PERIOD = 150;

	/// <summary>
	/// Report (average) send back off period.
	/// </summary>
	static constexpr uint8_t REPORT_RESEND_PERIOD = 40;

	/// <summary>
	/// How many microseconds is the client allowed to tune its clock on one tune request. 
	/// </summary>
	static constexpr int16_t CLOCK_TUNE_RANGE_MICROS = 250;

	static constexpr uint32_t CLOCK_TUNE_PERIOD = 666;

	enum class LinkType : uint8_t
	{
		PublicKeyExchange = 0xBE,
		AddressMatch = 0xAE
	};

private:
	/// <summary>
	/// Abstract struct.
	/// ||SessionId|CompressedPublicKey||
	/// </summary>
	template<const uint8_t Header>
	struct PkeDefinition : public TemplateHeaderDefinition<Header, SESSION_ID_SIZE + LoLaCryptoDefinition::COMPRESSED_KEY_SIZE>
	{
		static constexpr uint8_t PAYLOAD_SESSION_ID_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_PUBLIC_KEY_INDEX = PAYLOAD_SESSION_ID_INDEX + SESSION_ID_SIZE;
	};

	/// <summary>
	/// ||SessionId|ServerPublicAddress||
	/// </summary>
	template<const uint8_t Header, const uint8_t ExtraSize>
	struct PaeBroadcastDefinition : public TemplateHeaderDefinition<Header, SESSION_ID_SIZE + LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_SESSION_ID_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_SERVER_ADDRESS_INDEX = PAYLOAD_SESSION_ID_INDEX + SESSION_ID_SIZE;
	};

	/// <summary>
	/// Abstract struct.
	/// ||RequestId||
	/// </summary>
	template<const uint8_t Header, const uint8_t ExtraSize>
	struct SwitchOverDefinition : public TemplateHeaderDefinition<Header, 1 + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_REQUEST_ID_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;

	};

	/// <summary>
	/// Abstract struct with full timestamp.
	/// ||RequestId|Seconds|SubSeconds|||
	/// </summary>
	template<const uint8_t Header, const uint8_t ExtraSize = 0>
	struct ClockSyncDefinition : public TemplateHeaderDefinition<Header, 1 + (TIME_SIZE * 2) + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_REQUEST_ID_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_SECONDS_INDEX = PAYLOAD_REQUEST_ID_INDEX + 1;
		static constexpr uint8_t PAYLOAD_SUB_SECONDS_INDEX = PAYLOAD_SECONDS_INDEX + TIME_SIZE;
	};

public:
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

		//_____________Address Match LINK______________
		/// <summary>
		/// ||||
		/// Request server to start an AM session.
		/// TODO: Add support for search for specific Device Id.
		/// </summary>
		using AmSessionRequest = TemplateHeaderDefinition<SearchReply::HEADER + 1, 0>;

		/// <summary>
		/// ||SessionId|PublicAddress||
		/// </summary>
		using AmSessionAvailable = PaeBroadcastDefinition<AmSessionRequest::HEADER + 1, 0>;

		/// <summary>
		/// ||SessionId|ServerPublicAddress|ClientPublicAddress||
		/// </summary>
		struct AmLinkingStartRequest : public PaeBroadcastDefinition<AmSessionAvailable::HEADER + 1, LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE>
		{
			using BaseClass = PaeBroadcastDefinition<AmSessionAvailable::HEADER + 1, LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE>;

			static constexpr uint8_t PAYLOAD_CLIENT_ADDRESS_INDEX = BaseClass::PAYLOAD_SERVER_ADDRESS_INDEX + LoLaCryptoDefinition::PUBLIC_ADDRESS_SIZE;
		};
		//_______________________________________________________


		//_______________Public Key Exchange LINK________________
		/// <summary>
		/// ||||
		/// Request server to start a PKE session.
		/// TODO: Add support for search for specific Device Id.
		/// </summary>
		using PkeSessionRequest = TemplateHeaderDefinition<AmLinkingStartRequest::HEADER + 1, 0>;

		/// <summary>
		/// ||SessionId|CompressedServerPublicKey||
		/// </summary>
		using PkeSessionAvailable = PkeDefinition<PkeSessionRequest::HEADER + 1>;

		/// <summary>
		/// ||SessionId|CompressedClientPublicKey||
		/// </summary>
		using PkeLinkingStartRequest = PkeDefinition<PkeSessionAvailable::HEADER + 1>;
		//_______________________________________________________


		/// <summary>
		/// ||RequestId|Remaining|Token||
		/// </summary>
		struct LinkingTimedSwitchOver : public SwitchOverDefinition<PkeLinkingStartRequest::HEADER + 1, TIME_SIZE + LINKING_TOKEN_SIZE>
		{
			using BaseClass = SwitchOverDefinition<PkeLinkingStartRequest::HEADER + 1, TIME_SIZE + LINKING_TOKEN_SIZE>;

			static constexpr uint8_t PAYLOAD_TIME_INDEX = BaseClass::PAYLOAD_REQUEST_ID_INDEX + 1;

			static constexpr uint8_t PAYLOAD_SESSION_TOKEN_INDEX = PAYLOAD_TIME_INDEX + TIME_SIZE;

		};

		/// <summary>
		/// ||RequestId|SessionToken||
		/// </summary>
		struct LinkingTimedSwitchOverAck : public SwitchOverDefinition<LinkingTimedSwitchOver::HEADER + 1, LINKING_TOKEN_SIZE>
		{
			using BaseClass = SwitchOverDefinition<LinkingTimedSwitchOver::HEADER + 1, LINKING_TOKEN_SIZE>;

			static constexpr uint8_t PAYLOAD_SESSION_TOKEN_INDEX = BaseClass::PAYLOAD_REQUEST_ID_INDEX + 1;
		};
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
		/// ||RequestId|Seconds|SubSeconds|||
		/// Seconds in full UTC seconds.
		/// Sub-seconds in microseconds [-999999; +999999]
		/// </summary>
		using ClockSyncRequest = ClockSyncDefinition<ServerChallengeReply::HEADER + 1>;

		/// <summary>
		/// ||RequestId|Seconds|SubSecondsError|Accepted||
		/// Sub-seconds in microseconds [-999999; +999999]
		/// Accepted true if non-zero.
		/// </summary>
		struct ClockSyncReply : public ClockSyncDefinition<ClockSyncRequest::HEADER + 1, 1>
		{
			using BaseClass = ClockSyncDefinition<ClockSyncRequest::HEADER + 1, 1>;

			static constexpr uint8_t PAYLOAD_ACCEPTED_INDEX = BaseClass::PAYLOAD_SUB_SECONDS_INDEX + TIME_SIZE;
		};

		/// <summary>
		/// 
		/// </summary>
		using StartLinkRequest = TemplateHeaderDefinition<ClockSyncReply::HEADER + 1, 0>;

		/// <summary>
		/// ||RequestId|Remaining||
		/// </summary>
		struct LinkTimedSwitchOver : public SwitchOverDefinition<StartLinkRequest::HEADER + 1, TIME_SIZE>
		{
			using BaseClass = SwitchOverDefinition<StartLinkRequest::HEADER + 1, TIME_SIZE>;

			static constexpr uint8_t PAYLOAD_TIME_INDEX = BaseClass::PAYLOAD_REQUEST_ID_INDEX + 1;
		};

		/// <summary>
		/// ||RequestId||
		/// </summary>
		using LinkTimedSwitchOverAck = SwitchOverDefinition<LinkTimedSwitchOver::HEADER + 1, 0>;
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
		struct ClockTuneRequest : public TemplateHeaderDefinition<ReportUpdate::HEADER + 1, TIME_SIZE>
		{
			static constexpr uint8_t PAYLOAD_ROLLING_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		};

		/// <summary>
		/// ||Rolling Time error (us)||
		/// </summary>
		struct ClockTuneReply : public TemplateHeaderDefinition<ClockTuneRequest::HEADER + 1, TIME_SIZE>
		{
			static constexpr uint8_t PAYLOAD_ERROR_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		};
	};

	/// <summary>
	/// Largest Link Packets will be the Broadcast packets, that contain the SessionId+PublicKey(compressed).
	/// </summary>
	static constexpr uint8_t LARGEST_PAYLOAD = PkeDefinition<0>::PAYLOAD_SIZE;
};
#endif