// LoLaLinkInfo.h

#ifndef _LOLALINK_INFO_h
#define _LOLALINK_INFO_h

#include <ILoLa.h>

#define LOLA_LINK_INFO_MAX_STALENESS	3000

class LoLaLinkInfo : public ILinkActiveIndicator
{
public:
	enum LinkStateEnum : uint8_t
	{
		Disabled = 0,
		Setup = 1,
		AwaitingLink = 2,
		AwaitingSleeping = 3,
		Connecting = 4,
		Connected = 5
	};

private:
	ILoLa * Driver = nullptr;
	uint32_t ActivityElapsedHelper = ILOLA_INVALID_MILLIS;

	uint16_t RTT = ILOLA_INVALID_LATENCY;
	uint32_t LinkStarted = ILOLA_INVALID_MILLIS;

	//Callback handler.
	Signal<const LinkStateEnum> LinkStatusUpdated;

	LinkStateEnum LinkState;

public:
	LinkStateEnum GetLinkState()
	{
		return LinkState;
	}

	void SetDriver(ILoLa* driver)
	{
		Driver = driver;
	}

	void Reset()
	{
		LinkState = LinkStateEnum::Disabled;
		RTT = ILOLA_INVALID_LATENCY;
		LinkStarted = ILOLA_INVALID_MILLIS;
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
		return LinkState == LinkStateEnum::Connected;
	}

	bool HasLatency()
	{
		return RTT != ILOLA_INVALID_LATENCY;
	}

	void SetRTT(const uint16_t rtt)
	{
		RTT = rtt;
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
			return Driver->GetMillis() - LinkStarted;
		}
		else
		{
			return 0;//Unknown.
		}
	}

	void ResetLinkStarted()
	{
		LinkStarted = ILOLA_INVALID_MILLIS;
	}

	void StampLinkStarted()
	{
		LinkStarted = Driver->GetMillis();
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
			return (int8_t)map(constrain(Driver->GetLastValidRSSI(), Driver->GetRSSIMin(), Driver->GetRSSIMax()),
				Driver->GetRSSIMin(), Driver->GetRSSIMax(),
				0, UINT8_MAX);
		}
		return 0;//Unknown.
	}

	//Output normalized to uint8_t range.
	uint8_t GetLinkFreshnesss()
	{
		ActivityElapsedHelper = GetLastReceivedElapsed();
		if (ActivityElapsedHelper != UINT32_MAX)
		{
			return map(min(LOLA_LINK_INFO_MAX_STALENESS, ActivityElapsedHelper), 0, LOLA_LINK_INFO_MAX_STALENESS, UINT8_MAX, 0);
		}

		return 0;//Not fresh at all.
	}

	//Output normalized to uint8_t range.
	uint32_t GetLastActivityElapsed()
	{
		if (Driver != nullptr)
		{
			if (Driver->GetLastSentMillis() >= Driver->GetLastValidReceivedMillis())
			{
				return Driver->GetMillis() - Driver->GetLastSentMillis();
			}
			else
			{
				return Driver->GetMillis() - Driver->GetLastValidReceivedMillis();
			}
		}
		return ILOLA_INVALID_MILLIS;
	}

	uint32_t GetLastSentElapsed()
	{
		if (Driver != nullptr && Driver->GetLastSentMillis() != ILOLA_INVALID_MILLIS)
		{
			return Driver->GetMillis() - Driver->GetLastSentMillis();
		}
		return ILOLA_INVALID_MILLIS;
	}

	uint32_t GetLastReceivedElapsed()
	{
		if (Driver != nullptr && Driver->GetLastValidReceivedMillis() != ILOLA_INVALID_MILLIS)
		{
			return Driver->GetMillis() - Driver->GetLastValidReceivedMillis();
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