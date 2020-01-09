// LoLaLinkInfo.h

#ifndef _LOLALINK_INFO_h
#define _LOLALINK_INFO_h

#include <ILoLaDriver.h>

#include <RingBufCPP.h>
#include <LinkIndicator.h>
#include <LoLaClock\ILoLaClockSource.h>
#include <LoLaCrypto\PseudoMacGenerator.h>


class LoLaLinkInfo : public LinkIndicator
{
private:
	const uint32_t ILOLA_INVALID_LATENCY = UINT32_MAX;
	const uint8_t INVALID_SESSION = 0;

	ILoLaDriver* Driver = nullptr;
	ILoLaClockSource* SyncedClock = nullptr;

	uint32_t LinkStartedSeconds = 0;

	//Real time update tracking.
	uint32_t PartnerInfoLastUpdated = 0;
	uint32_t PartnerInfoLastUpdatedRemotely = 0;

	RingBufCPP<uint8_t, RADIO_POWER_BALANCER_RSSI_SAMPLE_COUNT> PartnerRSSISamples;
	uint32_t PartnerAverageRSSI = 0;
	uint8_t PartnerReceivedCount = 0;

	//Link session information.
	uint8_t SessionId = INVALID_SESSION;

	//MAC generated from UUID.
	LoLaMAC<LOLA_LINK_INFO_MAC_LENGTH> LocalMacGenerator;
	//

	bool PartnerIdPresent = false;
	uint32_t PartnerId = 0;


	uint32_t ClockSyncAdjustments = 0;

	//Helper.
	uint32_t ActivityElapsedHelper = 0;


public:
	LoLaLinkInfo(ILoLaDriver* driver, ILoLaClockSource* syncedClock) :
		LinkIndicator(),
		LocalMacGenerator()
	{
		Driver = driver;
		SyncedClock = syncedClock;
		if (Driver != nullptr)
		{
			Driver->SetLinkStatusIndicator(this);
		}
	}

	//bool IsDisabled()
	//{
	//	return LinkState == StateEnum::Disabled;
	//}

	uint32_t GetPartnerId()
	{
		return PartnerId;
	}

	uint8_t* GetLocalMAC()
	{
		return LocalMacGenerator.GetMACPointer();
	}

	uint32_t GetLocalId()
	{
		return LocalMacGenerator.GetMACHash();
	}

	//Only for use in Host
	void ClearRemoteId()
	{
		PartnerIdPresent = false;
	}

	void SetPartnerId(const uint32_t id)
	{
		PartnerId = id;
		PartnerIdPresent = true;
	}

	bool HasPartnerId()
	{
		return PartnerId;
	}

	bool SetSessionId(const uint8_t sessionId)
	{
		SessionId = sessionId;

		return SessionId != INVALID_SESSION;
	}

	bool HasSession()
	{
		return HasSessionId() && HasPartnerId();
	}

	bool HasSessionId()
	{
		return SessionId != INVALID_SESSION;
	}

	uint8_t GetSessionId()
	{
		return SessionId;
	}

	uint32_t GetClockSyncAdjustments()
	{
		return ClockSyncAdjustments;
	}

	void StampClockSyncAdjustment()
	{
		ClockSyncAdjustments++;
	}

	void StampPartnerInfoUpdated()
	{
		PartnerInfoLastUpdated = millis();
	}

	void StampLocalInfoLastUpdatedRemotely()
	{
		PartnerInfoLastUpdatedRemotely = millis();
	}

	uint32_t GetLocalInfoUpdateRemotelyElapsed()
	{
		if (PartnerInfoLastUpdatedRemotely != 0)
		{
			return millis() - PartnerInfoLastUpdatedRemotely;
		}

		return UINT32_MAX;
	}

	uint32_t GetPartnerLastReportElapsed()
	{
		if (PartnerInfoLastUpdated != 0)
		{
			return millis() - PartnerInfoLastUpdated;
		}

		return UINT32_MAX;
	}

	void Reset()
	{
		SessionId = INVALID_SESSION;
		PartnerIdPresent = false;

		LinkStartedSeconds = 0;

		while (!PartnerRSSISamples.isEmpty())
		{
			PartnerRSSISamples.pull();
		}

		PartnerReceivedCount = 0;

		ClockSyncAdjustments = 0;

		if (Driver != nullptr)
		{
			Driver->ResetLiveData();
		}
	}

	uint32_t GetLinkDurationSeconds()
	{
		if (HasLink())
		{
			return (SyncedClock->GetSyncSeconds() - LinkStartedSeconds);
		}
		else
		{
			return 0;// Unknown.
		}
	}

	void StampLinkStarted()
	{
		LinkStartedSeconds = SyncedClock->GetSyncSeconds();
	}

	bool HasPartnerRSSI()
	{
		return !PartnerRSSISamples.isEmpty();
	}

	uint8_t GetPartnerRSSINormalized()
	{
		if (PartnerRSSISamples.isEmpty())
		{
			return 0;
		}
		else
		{
			PartnerAverageRSSI = 0;

			for (uint8_t i = 0; i < PartnerRSSISamples.numElements(); i++)
			{
				PartnerAverageRSSI += *PartnerRSSISamples.peek(i);
			}

			return PartnerAverageRSSI / PartnerRSSISamples.numElements();
		}
		return PartnerAverageRSSI;
	}

	void SetPartnerRSSINormalized(const uint8_t rssiNormalized)
	{
		PartnerRSSISamples.addForce(rssiNormalized);
	}

	uint8_t GetLostCount()
	{
		return max(PartnerReceivedCount, (Driver->GetTransmitedCount() % UINT8_MAX)) - PartnerReceivedCount;
	}

	uint8_t GetPartnerReceivedCount()
	{
		return PartnerReceivedCount;
	}

	void SetPartnerReceivedCount(const uint8_t partnerReceivedCount)
	{
		PartnerReceivedCount = partnerReceivedCount;
	}

	//Output normalized to uint8_t range.
	uint8_t GetRSSINormalized()
	{
		if (Driver != nullptr)
		{
			return Driver->GetRSSINormalized();
		}
		return 0;//Unknown.
	}

	uint32_t GetLastActivityElapsedMillis()
	{
		if (Driver != nullptr)
		{
			if (Driver->GetElapsedMillisLastValidSent() < Driver->GetElapsedMillisLastValidReceived())
			{
				return Driver->GetElapsedMillisLastValidSent();
			}
			else
			{
				return Driver->GetElapsedMillisLastValidReceived();
			}
		}
		return UINT32_MAX;
	}

	uint32_t GetElapsedMillisLastValidSent()
	{
		if (Driver != nullptr)
		{
			return Driver->GetElapsedMillisLastValidSent();
		}
		return UINT32_MAX;
	}

	uint32_t GetElapsedMillisLastValidReceived()
	{
		if (Driver != nullptr)
		{
			return Driver->GetElapsedMillisLastValidReceived();
		}
		return UINT32_MAX;
	}

	uint8_t GetTransmitPowerNormalized()
	{
		if (Driver != nullptr)
		{
			return Driver->GetTransmitPowerNormalized();
		}
		return 0;// Unknown.
	}

	// Public getters.
	uint64_t GetReceivedCount()
	{
		return Driver->GetReceivedCount();
	}

	uint64_t GetRejectedCount()
	{
		return Driver->GetRejectedCount();
	}

	uint64_t GetTransmitedCount()
	{
		return Driver->GetTransmitedCount();
	}

#ifdef DEBUG_LOLA
	uint64_t GetTimingCollisionCount()
	{
		return Driver->GetTimingCollisionCount();
	}

	uint64_t GetStateCollisionCount()
	{
		return Driver->GetTimingCollisionCount();
	}

	void PrintMac(Stream* serial)
	{
		PrintMac(serial, GetLocalMAC());
	}

	void PrintMac(Stream* serial, uint8_t* mac)
	{
		for (uint8_t i = 0; i < LOLA_LINK_INFO_MAC_LENGTH; i++)
		{
			serial->print(mac[i], HEX);
			if (i < LOLA_LINK_INFO_MAC_LENGTH - 1)
			{
				serial->print(':');
			}
		}
	}
#endif // DEBUG_LOLA
};
#endif