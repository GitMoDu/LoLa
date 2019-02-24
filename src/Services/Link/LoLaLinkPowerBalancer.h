// LoLaLinkPowerBalancer.h

#ifndef _LOLA_LINK_POWER_BALANCER_h
#define _LOLA_LINK_POWER_BALANCER_h

#include <ILoLa.h>
#include <Services\Link\LoLaLinkDefinitions.h>

#define RADIO_POWER_BALANCER_UPDATE_PERIOD				(uint32_t)500
#define RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN			(uint8_t)20
#define RADIO_POWER_BALANCER_ADJUST_UP_LONG				(uint8_t)(12)
#define RADIO_POWER_BALANCER_ADJUST_DOWN_LONG			(uint8_t)(8)
#define RADIO_POWER_BALANCER_POWER_MIN					(uint8_t)(35)
#define RADIO_POWER_BALANCER_POWER_MAX					(uint8_t)(UINT8_MAX)
#define RADIO_POWER_BALANCER_RSSI_TARGET				(uint8_t)(140)



class LoLaLinkPowerBalancer
{
private:
	LoLaLinkInfo* LinkInfo = nullptr;
	ILoLa* LoLa = nullptr;

	uint8_t CurrentPartnerRSSI = 0;

	uint8_t TransmitPowerNormalized = 0;

	uint32_t LastUpdated = ILOLA_INVALID_MILLIS;

public:
	LoLaLinkPowerBalancer() {}

	bool Setup(ILoLa* loLa, LoLaLinkInfo* linkInfo)
	{
		LoLa = loLa;
		LinkInfo = linkInfo;
		TransmitPowerNormalized = 0;

		LastUpdated = ILOLA_INVALID_MILLIS;

		return LoLa != nullptr && LinkInfo != nullptr;
	}

	bool Update()
	{
		if (LastUpdated == ILOLA_INVALID_MILLIS ||
			(millis() - LastUpdated > LOLA_LINK_SERVICE_LINKED_POWER_UPDATE_PERIOD))
		{
			CurrentPartnerRSSI = LinkInfo->GetPartnerRSSINormalized();

			LastUpdated = millis();

			uint8_t PreviousTransmitPower = TransmitPowerNormalized;

			if ((CurrentPartnerRSSI > RADIO_POWER_BALANCER_RSSI_TARGET) &&
				(TransmitPowerNormalized > RADIO_POWER_BALANCER_POWER_MIN))
			{
				//Down
				if (CurrentPartnerRSSI > (RADIO_POWER_BALANCER_RSSI_TARGET + RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN))
				{
					TransmitPowerNormalized = max(RADIO_POWER_BALANCER_POWER_MIN, TransmitPowerNormalized - RADIO_POWER_BALANCER_ADJUST_DOWN_LONG);
				}
				else if (TransmitPowerNormalized > RADIO_POWER_BALANCER_POWER_MIN)
				{
					TransmitPowerNormalized--;
				}
			}
			else if ((CurrentPartnerRSSI < RADIO_POWER_BALANCER_RSSI_TARGET) &&
				(TransmitPowerNormalized < RADIO_POWER_BALANCER_POWER_MAX))
			{
				//Up.
				if (CurrentPartnerRSSI < (RADIO_POWER_BALANCER_RSSI_TARGET - RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN))
				{
					TransmitPowerNormalized = min(RADIO_POWER_BALANCER_POWER_MAX, TransmitPowerNormalized + RADIO_POWER_BALANCER_ADJUST_UP_LONG);

				}
				else if (TransmitPowerNormalized < RADIO_POWER_BALANCER_POWER_MAX)
				{
					TransmitPowerNormalized++;
				}
			}

			if (PreviousTransmitPower != TransmitPowerNormalized)
			{
				LoLa->SetTransmitPower(TransmitPowerNormalized);

				return true;
			}
		}

		return false;
	}

	bool SetMaxPower()
	{
		if (LoLa != nullptr)
		{
			TransmitPowerNormalized = RADIO_POWER_BALANCER_POWER_MAX;
			LoLa->SetTransmitPower(TransmitPowerNormalized);

			return true;
		}

		return false;
	}

private:
	bool Up()
	{


		return false;
	}

	bool Down()
	{


		return false;
	}
};
#endif

