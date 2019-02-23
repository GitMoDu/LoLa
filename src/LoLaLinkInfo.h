// LoLaLinkInfo.h

#ifndef _LOLALINK_INFO_h
#define _LOLALINK_INFO_h

#include <ILoLa.h>
#include <Callback.h>

#define LOLA_LINK_INFO_MAC_LENGTH		8 //Following MAC-64, because why not?

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
	const uint8_t LOLA_LINK_INFO_INVALID_SESSION = 0;

	ILoLa * Driver = nullptr;

	LinkStateEnum LinkState = LinkStateEnum::Disabled;

	uint16_t RTT = ILOLA_INVALID_LATENCY;
	uint32_t LinkStarted = ILOLA_INVALID_MILLIS;

	uint8_t PartnerRSSINormalized = ILOLA_INVALID_RSSI_NORMALIZED;

	//Stored outside, only referenced.
	uint8_t* LocalMAC = nullptr;

	//Link session information.
	uint8_t SessionId = LOLA_LINK_INFO_INVALID_SESSION;

	bool PartnerMACPresent = false;
	uint8_t PartnerMAC[LOLA_LINK_INFO_MAC_LENGTH];

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

	uint8_t* GetPartnerMAC()
	{
		return PartnerMAC;
	}

	uint8_t* GetLocalMAC()
	{
		return LocalMAC;
	}

	void SetLocalMAC(uint8_t * macArray)
	{
		LocalMAC = macArray;
	}

	void SetPartnerMAC(uint8_t * macArray)
	{
		for (uint8_t i = 0; i < LOLA_LINK_INFO_MAC_LENGTH; i++)
		{
			PartnerMAC[i] = macArray[i];
		}
		PartnerMACPresent = true;
	}

	bool HasPartnerMAC()
	{
		return PartnerMACPresent;
	}	
	
	bool HasLocalMAC()
	{
		return LocalMAC != nullptr;
	}

	bool SetSessionId(const uint8_t sessionId)
	{
		SessionId = sessionId;

		return SessionId != LOLA_LINK_INFO_INVALID_SESSION;
	}

	bool HasSessionId()
	{
		return SessionId != LOLA_LINK_INFO_INVALID_SESSION;
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

	void Reset()
	{
		SessionId = LOLA_LINK_INFO_INVALID_SESSION;
		PartnerMACPresent = false;
		
		LinkStarted = ILOLA_INVALID_MILLIS;
		
		RTT = ILOLA_INVALID_LATENCY;
		PartnerRSSINormalized = ILOLA_INVALID_RSSI_NORMALIZED;

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
		return PartnerRSSINormalized != ILOLA_INVALID_RSSI_NORMALIZED;
	}

	uint8_t GetPartnerRSSINormalized()
	{
		return PartnerRSSINormalized;
	}

	void SetPartnerRSSINormalized(const uint8_t rssiNormalized)
	{
		PartnerRSSINormalized = rssiNormalized;
	}

	int16_t GetRSSI()
	{
		if (Driver != nullptr)
		{
			return constrain(Driver->GetLastValidRSSI(), Driver->GetRSSIMin(), Driver->GetRSSIMax());
		}
		return 0;//Unknown.		
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
			return map(Driver->GetTransmitPower(), Driver->GetTransmitPowerMin(), Driver->GetTransmitPowerMax(), 0, UINT8_MAX);
		}
		return 0;//Unknown.
	}
};
#endif