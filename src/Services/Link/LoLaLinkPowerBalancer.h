// LoLaLinkPowerBalancer.h

#ifndef _LOLA_LINK_POWER_BALANCER_h
#define _LOLA_LINK_POWER_BALANCER_h

#include <ILoLaDriver.h>
#include <Services\Link\LoLaLinkDefinitions.h>

#define RADIO_POWER_BALANCER_UPDATE_PERIOD				(uint32_t)(500)
#define RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN			(uint8_t)(15)
#define RADIO_POWER_BALANCER_ADJUST_UP_LONG				(uint8_t)(20)
#define RADIO_POWER_BALANCER_ADJUST_DOWN_LONG			(uint8_t)(10)
#define RADIO_POWER_BALANCER_POWER_MIN					(uint8_t)(13) // ~5% 
#define RADIO_POWER_BALANCER_POWER_MAX					100//(uint8_t)(UINT8_MAX)
#define RADIO_POWER_BALANCER_RSSI_TARGET				(uint8_t)(140)



class LoLaLinkPowerBalancer
{
private:
	LoLaLinkInfo* LinkInfo = nullptr;
	ILoLaDriver* LoLaDriver = nullptr;

	uint8_t CurrentPartnerRSSI = 0;

	uint8_t TransmitPowerNormalized = 0;
	uint8_t NextTransmitPower = 0;

	uint32_t LastUpdated = ILOLA_INVALID_MILLIS;

public:
	LoLaLinkPowerBalancer() {}

	bool Setup(ILoLaDriver* driver, LoLaLinkInfo* linkInfo)
	{
		LoLaDriver = driver;
		LinkInfo = linkInfo;
		TransmitPowerNormalized = 0;

		LastUpdated = ILOLA_INVALID_MILLIS;

		return LoLaDriver != nullptr && LinkInfo != nullptr;
	}

	bool Update()
	{
		if (LastUpdated == ILOLA_INVALID_MILLIS || 
			LoLaDriver->GetElapsedMillisLastValidReceived() >= LOLA_LINK_SERVICE_LINKED_MAX_PANIC)
		{
			LastUpdated = millis();
			SetMaxPower();
		}
		else if (millis() - LastUpdated > LOLA_LINK_SERVICE_LINKED_POWER_UPDATE_PERIOD)
		{
			LastUpdated = millis();

			CurrentPartnerRSSI = LinkInfo->GetPartnerRSSINormalized();


			NextTransmitPower = TransmitPowerNormalized;

			if ((CurrentPartnerRSSI > RADIO_POWER_BALANCER_RSSI_TARGET) &&
				(NextTransmitPower > RADIO_POWER_BALANCER_POWER_MIN))
			{
				//Down.
				if (CurrentPartnerRSSI > (RADIO_POWER_BALANCER_RSSI_TARGET + RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN))
				{
					NextTransmitPower = max(RADIO_POWER_BALANCER_POWER_MIN, NextTransmitPower - RADIO_POWER_BALANCER_ADJUST_DOWN_LONG);
				}
				else if (NextTransmitPower > RADIO_POWER_BALANCER_POWER_MIN)
				{
					NextTransmitPower--;
				}
			}
			else if ((CurrentPartnerRSSI < RADIO_POWER_BALANCER_RSSI_TARGET) &&
				(NextTransmitPower < RADIO_POWER_BALANCER_POWER_MAX))
			{
				//Up.
				if (CurrentPartnerRSSI < (RADIO_POWER_BALANCER_RSSI_TARGET - RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN))
				{
					NextTransmitPower = min(RADIO_POWER_BALANCER_POWER_MAX, NextTransmitPower + RADIO_POWER_BALANCER_ADJUST_UP_LONG);

				}
				else if (NextTransmitPower < RADIO_POWER_BALANCER_POWER_MAX)
				{
					NextTransmitPower++;
				}
			}

			if (NextTransmitPower != TransmitPowerNormalized)
			{
				TransmitPowerNormalized = NextTransmitPower;
				LoLaDriver->SetTransmitPower(TransmitPowerNormalized);

				return true;
			}
		}

		return false;
	}

	void SetMaxPower()
	{
		TransmitPowerNormalized = RADIO_POWER_BALANCER_POWER_MAX;
		LoLaDriver->SetTransmitPower(TransmitPowerNormalized);
	}
};
#endif

