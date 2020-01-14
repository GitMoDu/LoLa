// LoLaLinkPowerBalancer.h

#ifndef _LOLA_LINK_POWER_BALANCER_h
#define _LOLA_LINK_POWER_BALANCER_h

#include <ILoLaDriver.h>
#include <Services\Link\LoLaLinkDefinitions.h>
#include <PacketDriver\ILoLaSelector.h>



class LoLaLinkPowerBalancer : public ITransmitPowerSelector
{
private:
	LoLaLinkInfo* LinkInfo = nullptr;
	ILoLaDriver* LoLaDriver = nullptr;

	uint8_t CurrentPartnerRSSI = 0;

	//uint8_t TransmitPowerNormalized = 0;
	uint8_t CurrentTransmitPower = 0;

	uint32_t LastUpdated = 0;

public:
	LoLaLinkPowerBalancer() 
		: ITransmitPowerSelector()
	{
	}

	uint8_t GetTransmitPowerNormalized()
	{
		return CurrentTransmitPower;
	}

	bool Setup(ILoLaDriver* driver, LoLaLinkInfo* linkInfo)
	{
		LoLaDriver = driver;
		LinkInfo = linkInfo;

		return LoLaDriver != nullptr && LinkInfo != nullptr;
	}

	bool Update()
	{
		if (LastUpdated == 0 ||
			LoLaDriver->GetElapsedMillisLastValidReceived() >= LOLA_LINK_SERVICE_LINKED_MAX_PANIC)
		{
			LastUpdated = millis();
			CurrentTransmitPower = RADIO_POWER_BALANCER_POWER_MAX;
			LoLaDriver->OnTransmitPowerUpdated();

			return true;
		}
		else if (millis() - LastUpdated > LOLA_LINK_SERVICE_LINKED_POWER_UPDATE_PERIOD)
		{
			LastUpdated = millis();

			CurrentPartnerRSSI = LinkInfo->GetPartnerRSSINormalized();

			if ((CurrentPartnerRSSI > RADIO_POWER_BALANCER_RSSI_TARGET) &&
				(CurrentTransmitPower > RADIO_POWER_BALANCER_POWER_MIN))
			{
				// Down.
				if (CurrentPartnerRSSI > (RADIO_POWER_BALANCER_RSSI_TARGET + RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN))
				{
					CurrentTransmitPower = max(RADIO_POWER_BALANCER_POWER_MIN, CurrentTransmitPower - RADIO_POWER_BALANCER_ADJUST_DOWN_LONG);
				}
				else if (CurrentTransmitPower > RADIO_POWER_BALANCER_POWER_MIN)
				{
					CurrentTransmitPower--;
				}
			}
			else if ((CurrentPartnerRSSI < RADIO_POWER_BALANCER_RSSI_TARGET) &&
				(CurrentTransmitPower < RADIO_POWER_BALANCER_POWER_MAX))
			{
				// Up.
				if (CurrentPartnerRSSI < (RADIO_POWER_BALANCER_RSSI_TARGET - RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN))
				{
					CurrentTransmitPower = min(RADIO_POWER_BALANCER_POWER_MAX, CurrentTransmitPower + RADIO_POWER_BALANCER_ADJUST_UP_LONG);

				}
				else if (CurrentTransmitPower < RADIO_POWER_BALANCER_POWER_MAX)
				{
					CurrentTransmitPower++;
				}
			}

			LoLaDriver->OnTransmitPowerUpdated();

			return true;
		}

		return false;
	}

	void SetMaxPower()
	{
		CurrentTransmitPower = RADIO_POWER_BALANCER_POWER_MAX;
		LoLaDriver->OnTransmitPowerUpdated();
	}
};
#endif

