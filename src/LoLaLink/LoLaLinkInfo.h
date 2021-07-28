// LoLaLinkInfo.h

#ifndef _LOLALINK_INFO_h
#define _LOLALINK_INFO_h

#include <LoLaDefinitions.h>
#include "LoLaLinkIndicator.h"
#include <PacketDriver\PacketIOInfo.h>

#include <LoLaClock\LoLaClock.h>
#include <LoLaCrypto\LoLaMAC.h>

#include <Link\ProtocolVersionCalculator.h>


class LoLaLinkInfo : public LoLaLinkIndicator
{
private:
	struct SessionInfoStruct
	{
		// Random salt, unique for each session.
		uint32_t SessionId = 0;
		uint32_t SessionIdLastSet = 0;

		// Signed session Id.
		uint32_t SessionIdSignature = 0;

		void Reset()
		{
			SessionIdLastSet = 0;
			SessionId = 0;
		}

		uint32_t GetElapsedSinceSet()
		{
			return millis() - SessionIdLastSet;
		}

		void SetSessionId(const uint32_t sessionId)
		{
			SessionId = sessionId;
			SessionIdLastSet = millis();
		}
	};

	struct ReportInfoStruct
	{
		// Last time the partner possibly got an update.
		uint32_t LastLocalSend = 0;

		// Last time we got a partner update.
		uint32_t LastPartnerUpdate = 0;

		// Last known partner RSSI.
		uint8_t PartnerRSSINormalized = 0;

		bool PartnerInfoPresent = false;

		uint8_t GetPartnerRSSINormalized()
		{
			if (PartnerInfoPresent)
			{
				return PartnerRSSINormalized;
			}
			else
			{
				return 0;
			}
		}

		uint32_t GetLastLocalSendElapsed()
		{
			return millis() - LastLocalSend;
		}

		void StampLocalSend()
		{
			LastLocalSend = millis();
		}

		void UpdatePartnerInfo(const uint8_t rssiNormalized)
		{
			PartnerInfoPresent = true;
			PartnerRSSINormalized = rssiNormalized;

			LastPartnerUpdate = millis();
		}

		uint32_t GetPartnerLastUpdateElapsed()
		{
			if (PartnerInfoPresent)
			{
				return millis() - LastPartnerUpdate;
			}
			else
			{
				return UINT32_MAX;
			}
		}

		void Reset()
		{
			PartnerInfoPresent = false;
		}
	};


	struct LinkStatisticsStruct
	{
		uint32_t LostReceivedCount = 0;
		uint32_t LostSentCount = 0;
		uint32_t LinkStartedSeconds = 0;


		void Reset()
		{
			LostReceivedCount = 0;
			LostSentCount = 0;
			LinkStartedSeconds = 0;
		}
	};
private:
	static const uint32_t ILOLA_INVALID_LATENCY = UINT32_MAX;
	static const uint8_t RADIO_POWER_BALANCER_RSSI_SAMPLE_COUNT = 3;


	PacketIOInfo* IOInfo = nullptr;
	ISyncedClock* Clock = nullptr;

	//// MAC generated from UUID (16 bytes).
	//static const uint8_t MACLength = 16;
	//LoLaMAC<MACLength> LocalMacGenerator;
	////

	SessionInfoStruct SessionInfo;
	ReportInfoStruct PartnerInfo;
	LinkStatisticsStruct LinkStatistics;

public:
	const uint32_t LinkProtocolVersion;

public:
	LoLaLinkInfo(PacketIOInfo* ioInfo, ISyncedClock* clock)
		: LoLaLinkIndicator()
		//, LocalMacGenerator()
		, SessionInfo()
		, PartnerInfo()
		, LinkStatistics()
		, IOInfo(ioInfo)
		, Clock(clock)
		, LinkProtocolVersion(ProtocolVersionCalculator::GetProtocolCode())
	{
	}

	void Reset()
	{
		IOInfo->Reset();
		SessionInfo.Reset();
		PartnerInfo.Reset();
		LinkStatistics.Reset();
	}

	const uint8_t GetSessionId()
	{
		return SessionInfo.SessionId;
	}

	void SetSessionId(const uint32_t sessionId)
	{
		SessionInfo.SetSessionId(sessionId);
	}
	
	const uint8_t GetSessionIdSignature()
	{
		return SessionInfo.SessionId;
	}

	void SetSessionIdSignature(const uint32_t sessionIdSignature)
	{
		SessionInfo.SessionIdSignature = sessionIdSignature;
	}

	const uint32_t GetElapsedMillisSinceIdLastSet()
	{
		return millis() - SessionInfo.SessionIdLastSet;
	}

	void OnCounterSkip(const uint8_t skips)
	{
		LinkStatistics.LostReceivedCount += skips;
	}

	void UpdatePartnerInfo(const uint8_t rssiNormalized)
	{
		PartnerInfo.UpdatePartnerInfo(rssiNormalized);
	}

	void StampLocalInfoSent()
	{
		PartnerInfo.StampLocalSend();
	}

	uint32_t GetLastLocalInfoSentElapsed()
	{
		return PartnerInfo.GetLastLocalSendElapsed();
	}

	uint32_t GetPartnerLastUpdateElapsed()
	{
		return PartnerInfo.GetPartnerLastUpdateElapsed();
	}

	uint32_t GetLinkDurationSeconds()
	{
		if (HasLink())
		{
			return (Clock->GetSyncSeconds() - LinkStatistics.LinkStartedSeconds);
		}
		else
		{
			return 0;
		}
	}

	void StampLinkStarted()
	{
		LinkStatistics.LinkStartedSeconds = Clock->GetSyncSeconds();
	}

	bool HasPartnerRSSI()
	{
		return PartnerInfo.PartnerInfoPresent;
	}

	uint8_t GetPartnerRSSINormalized()
	{
		return PartnerInfo.GetPartnerRSSINormalized();
	}

	// Easy getters to avoid using the whole IOInfo when you have LinkInfo.
	uint32_t GetLastActivityElapsedMillis()
	{
		return IOInfo->GetLastActivityElapsedMillis();
	}

	uint32_t GetElapsedMillisLastValidSent()
	{
		return IOInfo->GetElapsedMillisLastValidSent();
	}

	uint32_t GetElapsedMillisLastValidReceived()
	{
		return IOInfo->GetElapsedMillisLastValidReceived();
	}

	//#ifdef DEBUG_LOLA
	//	void PrintMac(Stream* serial)
	//	{
	//		LocalMacGenerator.PrintMac(serial);
	//	}
	//#endif // DEBUG_LOLA
};
#endif