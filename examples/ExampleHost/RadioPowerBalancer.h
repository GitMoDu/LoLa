// RadioPowerBalancer.h

#ifndef _RADIOPOWERBALANCER_h
#define _RADIOPOWERBALANCER_h

#include <RingBufCPP.h>

#define RADIO_POWER_BALANCER_UPDATE_PERIOD				(uint32_t)500
#define RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN			(int8) 20

#define RADIO_POWER_BALANCER_RSSI_SAMPLE_COUNT			3

#define RADIO_POWER_BALANCER_MAX_ELAPSED_BEFORE_BOOST	(uint32_t)2000

class RadioPowerBalancer
{
private:
	RingBufCPP<int8_t, RADIO_POWER_BALANCER_RSSI_SAMPLE_COUNT> RSSISamples;

	int32_t AverageRSSI;
	uint8_t ElementCount;

	ILoLa* LoLa;

public:
	RadioPowerBalancer(ILoLa* loLa)
	{
		LoLa = lola;
	}

	void AddRemoteRssiSample(const int8_t rsssi)
	{
		RSSISamples.addForce(rsssi);
	}

	void Update()
	{
		if (LoLa->GetMillis() - LoLa->GetLastValidReceivedMillis() > RADIO_POWER_BALANCER_MAX_ELAPSED_BEFORE_BOOST)
		{
			//In case of possibly failing communications, ramp up power.
			LoLa->SetTransmitPowerRatio(UINT8_MAX);
		}
		else
		{
			if (GetRSSIAverage() > LoLa->GetRSSIMin() + RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN)
			{
				Down();
			}
			else if (GetRSSIAverage() < LoLa->GetRSSIMin() + RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN)
			{
				Up();
			}
		}
	}

private:
	void Up()
	{
		if (LoLa->GetTransmitPowerRatio() < UINT8_MAX)
		{
			LoLa->SetTransmitPowerRatio(LoLa->GetTransmitPowerRatio() + 1);
		}		
	}

	void Down()
	{
		if (LoLa->GetTransmitPowerRatio() > 0)
		{
			LoLa->SetTransmitPowerRatio(LoLa->GetTransmitPowerRatio() + 1);
		}
	}

	int8_t GetRSSIAverage()
	{
		AverageRSSI = 0;
		ElementCount = 0;

		for (uint8_t i = 0; i < RSSISamples.numElements(); i++)
		{
			AverageRSSI += *RSSISamples.peek(i);
			ElementCount++;
		}

		if (ElementCount > 0)
		{
			return AverageRSSI / ElementCount;
		}
		else
		{

		}	
	}
};
#endif

