// LoLaLinkInfo.h

#ifndef _LOLALINK_INFO_h
#define _LOLALINK_INFO_h

#include <ILoLa.h>
#include <Callback.h>
#include <RingBufCPP.h>
#include <LoLaCrypto\PseudoMacGenerator.h>

#define LOLA_LINK_INFO_MAC_LENGTH		8 //Following MAC-64, because why not?

#define RADIO_POWER_BALANCER_RSSI_SAMPLE_COUNT			3


class LoLaLinkInfo
{
public:
	enum LinkStateEnum : uint8_t
	{
		Disabled = 0,
		Setup = 1,
		AwaitingLink = 2,
		AwaitingSleeping = 3,
		Linking = 4,
		Linked = 5
	};

private:
	const uint8_t INVALID_SESSION = 0;

	ILoLa * Driver = nullptr;

	LinkStateEnum LinkState = LinkStateEnum::Disabled;

	uint16_t RTT = ILOLA_INVALID_LATENCY;
	uint32_t LinkStarted = ILOLA_INVALID_MILLIS;

	//Real time update tracking.
	uint32_t PartnerInfoLastUpdated = ILOLA_INVALID_MILLIS;
	uint32_t PartnerInfoLastUpdatedRemotely = ILOLA_INVALID_MILLIS;

	RingBufCPP<uint8_t, RADIO_POWER_BALANCER_RSSI_SAMPLE_COUNT> PartnerRSSISamples;
	uint32_t PartnerAverageRSSI;

	//Link session information.
	uint8_t SessionId = INVALID_SESSION;

	//MAC generated from UUID.
	LoLaMAC<LOLA_LINK_INFO_MAC_LENGTH> LocalMacGenerator;
	//

	bool PartnerIdPresent = false;
	uint32_t PartnerId = 0;

	uint32_t ClockSyncAdjustments = 0;

	//Helper.
	uint32_t ActivityElapsedHelper = ILOLA_INVALID_MILLIS;

	//Callback handler.
	Signal<const LinkStateEnum> LinkStatusUpdated;


public:
	LinkStateEnum GetLinkState()
	{
		return LinkState;
	}

	uint32_t GetPartnerId()
	{
		return PartnerId;
	}

	//TODO: Implement on Link.
	/*bool LocalMACMatch(uint8_t * mac)
	{
		return LocalMacGenerator.Match(mac);
	}

	bool RemoteMACPartnerMatch(uint8_t * remoteMAC)
	{
		if (!PartnerMACPresent)
		{
			return false;
		}

		return LocalMacGenerator.Match(PartnerMAC, remoteMAC);
	}
	*/

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

	void SetDriver(ILoLa* driver)
	{
		Driver = driver;
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
		if (PartnerInfoLastUpdatedRemotely != ILOLA_INVALID_MILLIS)
		{
			return millis() - PartnerInfoLastUpdatedRemotely;
		}

		return ILOLA_INVALID_MILLIS;
	}

	uint32_t GetPartnerLastReportElapsed()
	{
		if (PartnerInfoLastUpdated != ILOLA_INVALID_MILLIS)
		{
			return millis() - PartnerInfoLastUpdated;
		}

		return ILOLA_INVALID_MILLIS;
	}

	void Reset()
	{
		SessionId = INVALID_SESSION;
		PartnerIdPresent = false;
		
		LinkStarted = ILOLA_INVALID_MILLIS;
		
		RTT = ILOLA_INVALID_LATENCY;
		while (!PartnerRSSISamples.isEmpty())
		{
			PartnerRSSISamples.pull();
		}

		ClockSyncAdjustments = 0;

#ifdef USE_LATENCY_COMPENSATION
		if (Driver != nullptr)
		{
			Driver->SetLinkStatus(false);
			Driver->ResetLiveData();
		}
#endif
	}

	void UpdateState(LinkStateEnum newState)
	{
		LinkState = newState;
		LinkStatusUpdated.fire(LinkState);
	}

	void AttachOnLinkStatusUpdated(const Slot<const LinkStateEnum>& slot)
	{
		LinkStatusUpdated.attach(slot);
	}

	bool IsDisabled()
	{
		return LinkState == LinkStateEnum::Disabled;
	}

	bool HasLink()
	{
		return LinkState == LinkStateEnum::Linked;
	}

	bool HasLatency()
	{
		return RTT != ILOLA_INVALID_LATENCY;
	}

	void SetRTT(const uint16_t rtt)
	{
		RTT = rtt;
#ifdef USE_LATENCY_COMPENSATION
		if (Driver != nullptr)
		{
			Driver->SetETTM((uint8_t)round((float)RTT / (float)2000));
		}
#endif
	}

	uint16_t GetRTT()
	{
		return RTT;
	}

	float GetLatency()
	{
		if (HasLatency())
		{
			return ((float)RTT / (float)2000);
		}
		else
		{
			return (float)0;//Unknown.
		}
	}

	uint32_t GetLinkDuration()
	{
		if (HasLink() && LinkStarted != ILOLA_INVALID_MILLIS)
		{
			return Driver->GetSyncMillis() - LinkStarted;
		}
		else
		{
			return 0;//Unknown.
		}
	}

	void StampLinkStarted()
	{
		if (Driver != nullptr)
		{
			LinkStarted = Driver->GetSyncMillis();
			Driver->SetLinkStatus(true);
		}
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

	//Output normalized to uint8_t range.
	uint8_t GetRSSINormalized()
	{
		if (Driver != nullptr)
		{
			return Driver->GetRSSINormalized();
		}
		return 0;//Unknown.
	}

	uint32_t GetLastActivityElapsed()
	{
		if (Driver != nullptr)
		{
			if (Driver->GetLastValidSentMillis() >= Driver->GetLastValidReceivedMillis())
			{
				return millis() - Driver->GetLastValidSentMillis();
			}
			else
			{
				return millis() - Driver->GetLastValidReceivedMillis();
			}
		}
		return ILOLA_INVALID_MILLIS;
	}

	uint32_t GetLastSentElapsed()
	{
		if (Driver != nullptr && Driver->GetLastValidSentMillis() != ILOLA_INVALID_MILLIS)
		{
			return millis() - Driver->GetLastValidSentMillis();
		}
		return ILOLA_INVALID_MILLIS;
	}

	uint32_t GetLastReceivedElapsed()
	{
		if (Driver != nullptr && Driver->GetLastValidReceivedMillis() != ILOLA_INVALID_MILLIS)
		{
			return millis() - Driver->GetLastValidReceivedMillis();
		}
		return ILOLA_INVALID_MILLIS;
	}

	uint8_t GetTransmitPowerNormalized()
	{
		if (Driver != nullptr)
		{
			return Driver->GetTransmitPowerNormalized();
		}
		return 0;//Unknown.
	}

#ifdef DEBUG_LOLA
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