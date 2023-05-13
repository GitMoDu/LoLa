// LoLaLinkReportTracker.h

#ifndef _LOLA_LINK_REPORT_TRACKER_h
#define _LOLA_LINK_REPORT_TRACKER_h


#include <stdint.h>
//#include "LoLaLinkDefinition.h"

template<const uint32_t UpdatePeriod,
	const uint8_t ResendPeriod,
	const uint32_t SilenceMaxPeriod	>
class LoLaLinkReportTracker
{
private:
	/// <summary>
	/// Half the resend period, up or down.
	/// </summary>
	static constexpr uint8_t REPORT_SEND_JITTER_RANGE = ResendPeriod / 2;

	/// <summary>
	/// UpdatePeriod with a small tolerance for at least 2 sends.
	/// </summary>
	static constexpr uint32_t REPORT_UPDATE_TIMEOUT = UpdatePeriod + (ResendPeriod * 2);
	//static constexpr uint32_t REPORT_UPDATE_TIMEOUT = LoLaLinkDefinition::REPORT_UPDATE_PERIOD + (LoLaLinkDefinition::REPORT_RESEND_PERIOD * 2);

	/// <summary>
	/// Converts abstract jitterScale to delay period.
	/// </summary>
	/// <param name="jitterScale">[INT8_MIN, INT8_MAX]</param>
	/// <returns>Send jitter delay period.</returns>
	static constexpr uint32_t GetSendJitterPeriod(const int8_t jitterScale)
	{
		return 1 + (uint32_t)ResendPeriod + (((int32_t)jitterScale * ResendPeriod) / INT8_MAX);
	}

private:
	uint32_t LastSent = 0;
	uint32_t LastReceived = 0;

	int8_t SendJitterScale = 0;

	uint8_t Rssi = 0;

	bool Requested = false;

public:
	LoLaLinkReportTracker()
	{
	}

	void Reset()
	{
		LastSent = 0;
		LastReceived = 0;
		Requested = false;
	}

	const uint8_t GetReportedRssi()
	{
		return Rssi;
	}

public:
	void OnReportReceived(const uint32_t timestamp, const uint8_t rssi)
	{
		// Updating the latest received dismisses internal request for back report, as we just got one.
		LastReceived = timestamp;
		Rssi = rssi;
	}

	void RequestReportUpdate(const int8_t sendJitterScale)
	{
		Requested = true;
		SendJitterScale = sendJitterScale;
	}

	void OnReportSent(const uint32_t timestamp)
	{
		LastSent = timestamp;
		Requested = false;
	}

public:
	const bool IsRequested(const uint32_t timestamp)
	{
		return Requested;
	}

	const bool IsSendRequested(const uint32_t timestamp)
	{
		return Requested && (timestamp - LastSent) > GetSendJitterPeriod(SendJitterScale);
	}

	const bool IsReportNeeded(const uint32_t timestamp, const uint32_t elapsedSinceLastValidReceived)
	{
		return (timestamp - LastSent > UpdatePeriod)
			|| ((timestamp - LastSent > ResendPeriod)
				&& IsBackReportNeeded(timestamp, elapsedSinceLastValidReceived));
	}

	const bool IsBackReportNeeded(const uint32_t timestamp, const uint32_t elapsedSinceLastValidReceived)
	{
		//TODO: Check if the latest received RSSI is low enough to trigger a Report update request.
		return (timestamp - LastReceived > REPORT_UPDATE_TIMEOUT)
			|| elapsedSinceLastValidReceived > SilenceMaxPeriod;
	}
};
#endif