// LoLaLinkDefinition.h

#ifndef _LOLA_LINK_DEFINITION_h
#define _LOLA_LINK_DEFINITION_h

#include "LoLaPacketDefinition.h"

namespace LoLaLinkDefinition
{
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
	/// Random challenge to be solved by the partner,
	/// before granting access to next Linking Step..
	/// Uses a  hash with the Challenge and ACCESS_CONTROL_PASSWORD,
	/// instead of a slow (but more secure) certificate-signature.
	/// </summary>
	static constexpr uint8_t CHALLENGE_CODE_SIZE = 4;
	static constexpr uint8_t CHALLENGE_SIGNATURE_SIZE = 4;

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
	/// Link state transition durations in duplex counts.
	/// Transitions happen on pre-link duplex, which has a period of duplexPeriod x 2.
	/// </summary>
	static constexpr uint8_t LINKING_TRANSITION_DUPLEX_COUNT = 7;

	/// <summary>
	/// If linking stage is not complete after this time, restart.
	/// </summary>
	static constexpr uint8_t LINKING_STAGE_TIMEOUT_DUPLEX_COUNT = 15;
	static constexpr uint32_t LINKING_STAGE_TIMEOUT_MIN_MICROS = 25000;

	/// <summary>
	/// How long without an input message from partner before disconnect.
	/// </summary>
	static constexpr uint8_t LINK_STAGE_TIMEOUT_DUPLEX_COUNT = 50;
	static constexpr uint32_t LINK_STAGE_TIMEOUT_MIN_MICROS = 750000;

	/// <summary>
	/// Duplex periods over this value are too long for LoLa to work effectively.
	/// </summary>
	static constexpr uint16_t DUPLEX_PERIOD_MAX_MICROS = 50000;

	/// <summary>
	/// Report max update period in microseconds. Slow value, let the main services hog the link.
	/// </summary>
	static constexpr uint32_t REPORT_UPDATE_PERIOD_MICROS = 432100;

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

	/// <summary>
	/// State transitions depend on duplex period.
	/// </summary>
	/// <param name="duplexPeriod"></param>
	/// <returns>Link state transition duration in microseconds.</returns>
	static constexpr uint32_t GetTransitionDuration(const uint16_t duplexPeriod)
	{
		return (uint32_t)duplexPeriod * LINKING_TRANSITION_DUPLEX_COUNT;
	}

	/// <summary>
	/// Linking stages timeout depends on duplex period.
	/// </summary>
	/// <param name="duplexPeriod"></param>
	/// <returns></returns>
	static constexpr uint32_t GetLinkingStageTimeoutDuration(const uint16_t duplexPeriod)
	{
		return LINKING_STAGE_TIMEOUT_MIN_MICROS + (uint32_t)duplexPeriod * LINKING_STAGE_TIMEOUT_DUPLEX_COUNT;
	}

	/// <summary>
	/// Link timeout depends on duplex period.
	/// </summary>
	/// <param name="duplexPeriod"></param>
	/// <returns></returns>
	static const uint32_t GetLinkTimeoutDuration(const uint16_t duplexPeriod)
	{
		return LINK_STAGE_TIMEOUT_MIN_MICROS + (uint32_t)duplexPeriod * LINK_STAGE_TIMEOUT_DUPLEX_COUNT;
	}
};
#endif