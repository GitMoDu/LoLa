// ReportTracker.h

#ifndef _REPORT_TRACKER_h
#define _REPORT_TRACKER_h


#include <stdint.h>
#include "LoLaLinkDefinition.h"
#include "Quality\LinkQualityTracker.h"

class ReportTracker : public LinkQualityTracker
{
private:
	static constexpr uint8_t REPORT_RESEND_PERIOD = LoLaLinkDefinition::REPORT_UPDATE_PERIOD / 4;
	static constexpr uint8_t REPORT_SEND_BALANCING_DELAY = 7;
	static constexpr uint8_t REPORT_RESEND_URGENT_PERIOD = REPORT_RESEND_PERIOD / 3;
	static constexpr uint8_t PARTNER_SILENCE_WORST_QUALITY = 127;

private:
	uint32_t LastSent = 0;
	uint32_t LastReportReceived = 0;

	bool ReplyRequested = false;
	bool SendRequested = false;
	bool BalancingDelay = false;

public:
	ReportTracker(const uint32_t timeoutPeriod)
		: LinkQualityTracker(timeoutPeriod)
	{}

	void Reset(const uint32_t timestamp)
	{
		LastSent = 0;
		LastReportReceived = 0;
		SendRequested = false;
		ReplyRequested = false;

		ResetQuality(timestamp);
		ResetLastValidReceived(timestamp);

		RequestReportUpdate(true);
	}

	const bool CheckUpdate(const uint32_t timestamp)
	{
		CheckUpdateQuality(timestamp);

		if (!SendRequested)
		{
			if (ReplyRequested
				|| ((timestamp - LastSent) > LoLaLinkDefinition::REPORT_UPDATE_PERIOD)
				|| GetLastValidReceivedAgeQuality() < PARTNER_SILENCE_WORST_QUALITY)
			{
				SendRequested = true;

				// Shuffle balancing delay on and off.
				BalancingDelay = !BalancingDelay;
			}
		}

		return SendRequested;
	}

public:
	void OnReportReceived(const uint32_t timestamp, const uint16_t partnerLoopingDropCounter, const uint8_t partnerRssi, const bool replyBack)
	{
		// Updating the latest received dismisses internal request for back report, as we just got one.
		LastReportReceived = timestamp;

		TxDropRate.Accumulate(partnerLoopingDropCounter);
		TxRssi.Step(partnerRssi);

		// Check if partner is requesting a report back.
		ReplyRequested |= replyBack;
	}

	void RequestReportUpdate(const bool replyBack)
	{
		SendRequested = true;
		ReplyRequested |= replyBack;
	}

	void OnReportSent(const uint32_t timestamp)
	{
		LastSent = timestamp;
		SendRequested = false;
		ReplyRequested = false;
	}

public:
	const bool IsTimeToSendReport(const uint32_t timestamp)
	{
		if (IsBackReportNeeded(timestamp))
		{
			return (timestamp - LastSent) > REPORT_RESEND_URGENT_PERIOD;
		}
		else if (BalancingDelay)
		{
			return (timestamp - LastSent) > REPORT_RESEND_PERIOD + REPORT_SEND_BALANCING_DELAY;
		}
		else
		{
			return (timestamp - LastSent) > REPORT_RESEND_PERIOD;
		}
	}

	const bool IsReportSendRequested()
	{
		return SendRequested;
	}

	/// <summary>
	/// If the last received report has timed out.
	/// If no other service is getting messages.
	/// </summary>
	/// <param name="timestamp"></param>
	/// <returns></returns>
	const bool IsBackReportNeeded(const uint32_t timestamp)
	{
		return ((timestamp - LastReportReceived) > (LoLaLinkDefinition::REPORT_UPDATE_PERIOD + REPORT_RESEND_PERIOD))
			|| (GetElapsedSinceLastValidReceived() > LoLaLinkDefinition::REPORT_UPDATE_PERIOD);
	}
};
#endif