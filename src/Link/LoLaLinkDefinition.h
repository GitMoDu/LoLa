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
	static constexpr uint8_t LOLA_VERSION = 0;

	// <summary>
	/// When unlinked, top 2 ports are used.
	/// When linked, top port is reserved.
	/// </summary>
	static constexpr uint8_t LINK_PORT_ALLOCATION = 1;
	static constexpr uint8_t UN_LINKED_PORT_ALLOCATION = 2;
	static constexpr uint8_t MAX_DEFINITION_PORT = UINT8_MAX - LINK_PORT_ALLOCATION;

	/// <summary>
	/// 32 bit session id.
	/// </summary>
	static constexpr uint8_t SESSION_ID_SIZE = 3;

	/// <summary>
	/// 32 bit protocol id.
	/// </summary>
	static constexpr uint8_t PROTOCOL_ID_SIZE = 4;

	/// <summary>
	/// Implicit addressing key size.
	/// </summary>
	static constexpr uint8_t ADDRESS_KEY_SIZE = LoLaCryptoDefinition::CYPHER_IV_SIZE - LoLaCryptoDefinition::CYPHER_TAG_SIZE;

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

	/// <summary>
	/// Public key equivalent.
	/// </summary>
	static constexpr uint8_t PUBLIC_ADDRESS_SIZE = ACCESS_CONTROL_PASSWORD_SIZE;

	/// <summary>
	/// Shared secret key equivalent. 
	/// </summary>
	static constexpr uint8_t SECRET_KEY_SIZE = ACCESS_CONTROL_PASSWORD_SIZE;

public:
	/// <summary>
	/// The raw keys that are used during runtime encryption.
	/// These are filled in with HKDF from the shared secret key.
	/// </summary>
	struct ExpandedKeyStruct
	{
		/// <summary>
		/// Cypher Key.
		/// </summary>
		uint8_t CypherKey[LoLaCryptoDefinition::CYPHER_KEY_SIZE];

		/// <summary>
		/// Cypher Iv.
		/// </summary>
		uint8_t CypherIvSeed[ADDRESS_KEY_SIZE];

		/// <summary>
		/// Channel PRNG seed.
		/// </summary>
		uint8_t ChannelSeed[CHANNEL_KEY_SIZE];
	};

	static constexpr uint8_t HKDFSize = sizeof(ExpandedKeyStruct);

public:
	/// <summary>
	/// Link will tolerate some dropped packets by forwarding the rolling counter.
	/// </summary>
	static constexpr uint16_t ROLLING_COUNTER_ERROR = 500;

	/// <summary>
	/// Period in microseconds.
	/// Link switch over will tolerate a small error at the start.
	/// During link time, clock will be continously tuned for higher accuracy.
	/// </summary>
	static constexpr uint8_t LINKING_CLOCK_TOLERANCE = 25;

	/// <summary>
	/// Pre-link channels are spread over the pipe count.
	/// </summary>
	static constexpr uint8_t LINKING_ADVERTISING_PIPE_COUNT = 4;

	/// <summary>
	/// Link state transition durations in microseconds.
	/// </summary>
	static constexpr uint16_t LINKING_TRANSITION_PERIOD_MICROS = 35000;

	/// <summary>
	/// If linking is not complete after this time, unlink and restart.
	/// </summary>
	static constexpr uint16_t LINKING_STAGE_TIMEOUT = 300;

	/// <summary>
	/// How long without an input message from partner before disconnect.
	/// </summary>
	static constexpr uint16_t LINK_STAGE_TIMEOUT = 750;

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
	static constexpr uint16_t REPORT_UPDATE_PERIOD = 1000 / 4;

	enum class LinkType : uint8_t
	{
		PublicKeyExchange = 0xBE,
		AddressMatch = 0xAE
	};

	/// <summary>
	/// Limit the possible pre-link Advertising channels, BLE style.
	/// Spreads the pipes across the whole channel spectrum.
	/// </summary>
	/// <param name="abstractChannel"></param>
	/// <returns>Abstract Advertising Channel</returns>
	static const uint8_t GetAdvertisingChannel(const uint8_t abstractChannel)
	{
		// Mod the channel to get the selected pipe.
		const uint_fast16_t advertisingPipe = abstractChannel % LINKING_ADVERTISING_PIPE_COUNT;

		// Scale the pipe back to an abstract channel.
		return (advertisingPipe * UINT8_MAX) / (LINKING_ADVERTISING_PIPE_COUNT - 1);
	}

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

		static constexpr uint8_t COUNTER_SIZE = 2;

		/// <summary>
		/// ||ReplyRequested|RxRssiQuality|RxCounter||
		/// </summary>
		struct ReportUpdate : public TemplateHeaderDefinition<0, 1 + 1 + COUNTER_SIZE>
		{
			static constexpr uint8_t PAYLOAD_REQUEST_INDEX = SUB_PAYLOAD_INDEX;
			static constexpr uint8_t PAYLOAD_RSSI_INDEX = PAYLOAD_REQUEST_INDEX + 1;
			static constexpr uint8_t PAYLOAD_DROP_COUNTER_INDEX = PAYLOAD_RSSI_INDEX + 1;
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