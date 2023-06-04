// ReportTracker.h

#ifndef _REPORT_TRACKER_h
#define _REPORT_TRACKER_h


#include <stdint.h>
#include "LoLaLinkDefinition.h"

class ReportTracker
{
private:
	uint32_t LastSent = 0;
	uint32_t LastReceived = 0;

	uint8_t Rssi = 0;

	bool ReplyRequested = false;
	bool SendRequested = false;

public:
	ReportTracker()
	{}

	void Reset()
	{
		LastSent = 0;
		LastReceived = 0;
		SendRequested = false;
		ReplyRequested = false;
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

		ReplyRequested = false;
	}

	void RequestReportUpdate(const bool replyBack)
	{
		SendRequested = true;
		ReplyRequested = replyBack;
	}

	void OnReportSent(const uint32_t timestamp)
	{
		LastSent = timestamp;
		SendRequested = false;
	}

public:
	const bool IsTimeToSend(const uint32_t timestamp)
	{
		return (timestamp - LastSent) > LoLaLinkDefinition::REPORT_RESEND_PERIOD;
	}

	const bool IsSendRequested()
	{
		return SendRequested;
	}

	const bool IsReportNeeded(const uint32_t timestamp, const uint32_t elapsedSinceLastValidReceived)
	{
		if (!SendRequested)
		{
			if (IsTimeToSend(timestamp))
			{
				if (ReplyRequested)
				{
					SendRequested = true;
				}
				else
				{
					ReplyRequested = (timestamp - LastReceived) > (LoLaLinkDefinition::REPORT_UPDATE_PERIOD + LoLaLinkDefinition::REPORT_RESEND_PERIOD);
					ReplyRequested |= (elapsedSinceLastValidReceived > LoLaLinkDefinition::REPORT_PARTNER_SILENCE_TRIGGER_PERIOD);

					if (ReplyRequested || ((timestamp - LastSent) > LoLaLinkDefinition::REPORT_UPDATE_PERIOD))
					{
						SendRequested = true;
					}
				}
			}
		}

		return IsSendRequested();
	}

	const bool IsBackReportNeeded(const uint32_t timestamp, const uint32_t elapsedSinceLastValidReceived)
	{
		return ReplyRequested;
	}
};
#endif