// LoLaLinkDefinition.h

#ifndef _LOLA_LINK_DEFINITION_h
#define _LOLA_LINK_DEFINITION_h

#include "LoLaPacketDefinition.h"

/// <summary>
/// Link uses 3 registered ports:
/// - Unencrypted port for link start or quick re-link.
/// - Encrypted port. Tokens may change during linking.
/// - Link-time service port. Encrypted with token, as are all user services.
/// </summary>
namespace LoLaLinkDefinition
{
	enum class LinkType : uint8_t
	{
		PublicKeyExchange = 0xBE,
		AddressMatch = 0xAE
	};

	static constexpr uint8_t LOLA_VERSION = 0;

	/// <summary>
	/// Top port is reserved for Link.
	/// </summary>
	static constexpr uint8_t LINK_PORT = UINT8_MAX;
	static constexpr uint8_t MAX_DEFINITION_PORT = LINK_PORT - 1;

	/// <summary>
	/// 24 bit session id.
	/// </summary>
	static constexpr uint8_t SESSION_ID_SIZE = 3;

	/// <summary>
	/// 32 bit protocol id.
	/// </summary>
	static constexpr uint8_t PROTOCOL_ID_SIZE = 4;

	/// <summary>
	/// One time linking token.
	/// </summary>
	static constexpr uint8_t LINKING_TOKEN_SIZE = 4;

	/// <summary>
	/// A static password that's required to access a device.
	/// </summary>
	static constexpr uint8_t ACCESS_CONTROL_PASSWORD_SIZE = 8;

	/// <summary>
	/// Public key equivalent.
	/// </summary>
	static constexpr uint8_t PUBLIC_ADDRESS_SIZE = ACCESS_CONTROL_PASSWORD_SIZE;

	/// <summary>
	/// Shared secret key equivalent. 
	/// </summary>
	static constexpr uint8_t SECRET_KEY_SIZE = ACCESS_CONTROL_PASSWORD_SIZE;

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
};
#endif