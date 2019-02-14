// LoLaLinkPowerBalancer.h

#ifndef _LOLA_LINK_POWER_BALANCER_h
#define _LOLA_LINK_POWER_BALANCER_h

#include <ILoLa.h>
#include <RingBufCPP.h>

#define RADIO_POWER_BALANCER_UPDATE_PERIOD				(uint32_t)500
#define RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN			(int16) 5 //20

#define RADIO_POWER_BALANCER_RSSI_SAMPLE_COUNT			5

#define RADIO_POWER_BALANCER_MAX_ELAPSED_BEFORE_BOOST	(uint32_t)2000

class LoLaLinkPowerBalancer
{
private:
	RingBufCPP<int16_t, RADIO_POWER_BALANCER_RSSI_SAMPLE_COUNT> RSSISamples;

	int32_t AverageRSSI;

	ILoLa* LoLa = nullptr;

public:
	LoLaLinkPowerBalancer() {}

	bool Setup(ILoLa* loLa)
	{
		LoLa = loLa;

		ClearSamples();

		return LoLa != nullptr;
	}

	void Reset()
	{	
		SetMaxPower();
		ClearSamples();
	}

	void AddRemoteRssiSample(const int16_t rssi)
	{
		RSSISamples.addForce(rssi);
	}

	void OnLinkWarningHigh()
	{
		//In case of possibly failing communications, ramp up power.
		SetMaxPower();
	}

	void Update()
	{
		if (!LoLa->HasLink())
		{
			//When we're not connected, ramp up power.
			SetMaxPower();
		}
		else if (LoLa->GetMillis() - LoLa->GetLastValidReceivedMillis() > RADIO_POWER_BALANCER_MAX_ELAPSED_BEFORE_BOOST)
		{
			//In case of possibly failing communications, ramp up power.
			SetMaxPower();
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
	void ClearSamples()
	{
		while (!RSSISamples.isEmpty())
		{
			RSSISamples.pull();
		}
	}
	
	void SetMaxPower()
	{
		/*LoLa->SetTransmitPower(LoLa->GetTransmitPowerMax());*/
		//TODO: Remove: only for test boards.
		LoLa->SetTransmitPower((LoLa->GetTransmitPowerMax() + LoLa->GetTransmitPowerMin() )/2);
	}

	void Up()
	{
		if (LoLa->GetTransmitPower() < LoLa->GetTransmitPowerMax())
		{
			LoLa->SetTransmitPower(LoLa->GetTransmitPower() + 1);
		}
	}

	void Down()
	{
		if (LoLa->GetTransmitPower() > LoLa->GetTransmitPowerMin())
		{
			LoLa->SetTransmitPower(LoLa->GetTransmitPower() - 1);
		}
	}

	int16_t GetRSSIAverage()
	{
		if (RSSISamples.isEmpty())
		{
			return ILOLA_INVALID_RSSI;
		}
		else
		{
			AverageRSSI = 0;

			for (uint8_t i = 0; i < RSSISamples.numElements(); i++)
			{
				AverageRSSI += *RSSISamples.peek(i);
			}
			return AverageRSSI / RSSISamples.numElements();
		}
	}
};
#endif

