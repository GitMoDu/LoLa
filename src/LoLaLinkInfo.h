// LoLaLinkInfo.h

#ifndef _LOLALINK_INFO_h
#define _LOLALINK_INFO_h

#include <ILoLa.h>

#define LOLA_LINK_INFO_MAX_STALENESS	1000

#define LOLA_LINK_INFO_INVALID_RSSI		ILOLA_INVALID_RSSI
#define LOLA_LINK_INFO_INVALID_LATENCY	ILOLA_INVALID_MILLIS

class LoLaLinkInfo
{
private:
	ILoLa * Driver = nullptr;
	uint32_t ActivityElapsedHelper;

	uint16_t RTT = LOLA_LINK_INFO_INVALID_RSSI;
	uint32_t LinkStarted = 0;

public:
	enum LinkStateEnum
	{
		Disabled,
		Setup,
		AwaitingConnection,
		AwaitingSleeping,
		Connecting,
		Connected
	} LinkState;

	void SetDriver(ILoLa* driver)
	{
		Driver = driver;
	}

	void ResetLatency()
	{
		RTT = LOLA_LINK_INFO_INVALID_LATENCY;
	}
	
	bool HasLink()
	{
		return LinkState == LinkStateEnum::Connected;
	}

	bool HasLatency()
	{
		return RTT != LOLA_LINK_INFO_INVALID_LATENCY;
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
			return LOLA_LINK_INFO_INVALID_LATENCY;
		}
	}

	uint32_t GetLinkDuration()
	{
		if (LinkStarted != 0)
		{
			return Driver->GetMillis() - LinkStarted;
		}
		else
		{
			return 0;
		}
	}

	void ResetLinkStarted()
	{
		LinkStarted = 0;
	}

	void StampLinkStarted()
	{
		LinkStarted = Driver->GetMillis();
	}

	int16_t GetRSSI()
	{
		return constrain(Driver->GetLastRSSI(), Driver->GetRSSIMin(), Driver->GetRSSIMax());
	}

	//Output normalized to uint8_t range.
	uint8_t GetRSSINormalized()
	{
		return (int8_t)map(constrain(Driver->GetLastRSSI(), Driver->GetRSSIMin(), Driver->GetRSSIMax()),
			Driver->GetRSSIMin(), Driver->GetRSSIMax(),
			0, UINT8_MAX);
	}

	//Output normalized to uint8_t range.
	uint8_t GetLinkFreshnesss()
	{
		ActivityElapsedHelper = GetLastActivityElapsed();
		if (ActivityElapsedHelper != UINT32_MAX)
		{
			return map(min(LOLA_LINK_INFO_MAX_STALENESS, ActivityElapsedHelper), 0, LOLA_LINK_INFO_MAX_STALENESS, UINT8_MAX, 0);
		}

		return UINT8_MAX;
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
		return UINT32_MAX;
	}

	uint32_t GetLastSentElapsed()
	{
		if (Driver != nullptr)
		{
			return Driver->GetMillis() - Driver->GetLastSentMillis();
		}
		return UINT32_MAX;
	}

	uint32_t GetLastReceivedElapsed()
	{
		if (Driver != nullptr)
		{
			return Driver->GetMillis() - Driver->GetLastValidReceivedMillis();
		}
		return UINT32_MAX;
	}

	uint8_t GetTransmitPowerNormalized()
	{
		if (Driver != nullptr)
		{
			return map(Driver->GetTransmitPower(), Driver->GetTransmitPowerMin(), Driver->GetTransmitPowerMax(), 0, UINT8_MAX);
		}
		return UINT8_MAX;
	}
};
#endif