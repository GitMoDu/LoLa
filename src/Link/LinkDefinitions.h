// LinkDefinitions.h

#ifndef _LINK_DEFINITIONS_h
#define _LINK_DEFINITIONS_h

#include "LoLaLinkDefinition.h"

/// <summary>
/// Link uses 3 registered ports:
/// - Unencrypted port for link start or quick re-link.
/// - Encrypted port. Tokens may change during linking.
/// - Link-time service port. Encrypted with token, as are all user services.
/// </summary>
namespace LinkDefinitions
{
	/// <summary>
	/// Timestamps are in 32 bit UTC seconds or microseconds.
	/// </summary>
	static constexpr uint8_t TIME_SIZE = 4;

	/// <summary>
	/// Full timestamps have 32 bit UTC seconds and 32 bit microseconds.
	/// </summary>
	static constexpr uint8_t TIME_FULL_SIZE = TIME_SIZE * 2;

	/// <summary>
	/// Abstract struct.
	/// ||SessionId|CompressedPublicKey||
	/// </summary>
	template<const uint8_t Header>
	struct PkeDefinition : public TemplateHeaderDefinition<Header, LoLaLinkDefinition::SESSION_ID_SIZE + LoLaCryptoDefinition::COMPRESSED_KEY_SIZE>
	{
		static constexpr uint8_t PAYLOAD_SESSION_ID_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_PUBLIC_KEY_INDEX = PAYLOAD_SESSION_ID_INDEX + LoLaLinkDefinition::SESSION_ID_SIZE;
	};

	/// <summary>
	/// ||SessionId|ServerPublicAddress||
	/// </summary>
	template<const uint8_t Header, const uint8_t ExtraSize>
	struct PaeBroadcastDefinition : public TemplateHeaderDefinition<Header, LoLaLinkDefinition::SESSION_ID_SIZE + LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_SESSION_ID_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_SERVER_ADDRESS_INDEX = PAYLOAD_SESSION_ID_INDEX + LoLaLinkDefinition::SESSION_ID_SIZE;
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
	struct ClockSyncDefinition : public TemplateHeaderDefinition<Header, 1 + TIME_FULL_SIZE + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_REQUEST_ID_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_SECONDS_INDEX = PAYLOAD_REQUEST_ID_INDEX + 1;
		static constexpr uint8_t PAYLOAD_SUB_SECONDS_INDEX = PAYLOAD_SECONDS_INDEX + TIME_SIZE;
	};

	struct Unlinked
	{
	public:
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
		struct AmLinkingStartRequest : public PaeBroadcastDefinition<AmSessionAvailable::HEADER + 1, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE>
		{
			using BaseClass = PaeBroadcastDefinition<AmSessionAvailable::HEADER + 1, LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE>;

			static constexpr uint8_t PAYLOAD_CLIENT_ADDRESS_INDEX = BaseClass::PAYLOAD_SERVER_ADDRESS_INDEX + LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE;
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
		struct LinkingTimedSwitchOver : public SwitchOverDefinition<PkeLinkingStartRequest::HEADER + 1, TIME_SIZE + LoLaLinkDefinition::LINKING_TOKEN_SIZE>
		{
			using BaseClass = SwitchOverDefinition<PkeLinkingStartRequest::HEADER + 1, TIME_SIZE + LoLaLinkDefinition::LINKING_TOKEN_SIZE>;

			static constexpr uint8_t PAYLOAD_TIME_INDEX = BaseClass::PAYLOAD_REQUEST_ID_INDEX + 1;

			static constexpr uint8_t PAYLOAD_SESSION_TOKEN_INDEX = PAYLOAD_TIME_INDEX + TIME_SIZE;

		};

		/// <summary>
		/// ||RequestId|SessionToken||
		/// </summary>
		struct LinkingTimedSwitchOverAck : public SwitchOverDefinition<LinkingTimedSwitchOver::HEADER + 1, LoLaLinkDefinition::LINKING_TOKEN_SIZE>
		{
			using BaseClass = SwitchOverDefinition<LinkingTimedSwitchOver::HEADER + 1, LoLaLinkDefinition::LINKING_TOKEN_SIZE>;

			static constexpr uint8_t PAYLOAD_SESSION_TOKEN_INDEX = BaseClass::PAYLOAD_REQUEST_ID_INDEX + 1;
		};
	};

	struct Linking
	{
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
		/// ||ReplyRequested|RxRssiQuality|RxCounter||
		/// </summary>
		struct ReportUpdate : public TemplateHeaderDefinition<0, 1 + 1 + LoLaPacketDefinition::ID_SIZE>
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
};
#endif