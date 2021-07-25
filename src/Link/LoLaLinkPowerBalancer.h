// LoLaLinkPowerBalancer.h

#ifndef _LOLA_LINK_POWER_BALANCER_h
#define _LOLA_LINK_POWER_BALANCER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <Link\LoLaLinkDefinitions.h>
#include <PacketDriver\BaseLoLaDriver.h>
#include <PacketDriver\LoLaPacketDriverSettings.h>


class LoLaTransmitPowerManager : protected Task
{
private:
	BaseLoLaDriver* LoLaDriver = nullptr;
	LoLaLinkInfo* LinkInfo = nullptr;

	//uint8_t TransmitPowerNormalized = 0;
	volatile uint8_t CurrentTransmitPower = 0;

	uint32_t LastUpdated = 0;

	LoLaPacketDriverSettings* Settings;

public:
	LoLaTransmitPowerManager(Scheduler* scheduler
		, BaseLoLaDriver* lolaDriver
		, LoLaPacketDriverSettings* settings)
		: Task(0, TASK_FOREVER, scheduler, false)
		, LoLaDriver(lolaDriver)
		, LinkInfo(&lolaDriver->LinkInfo)
		, Settings(settings)
	{
	}

	uint8_t GetTransmitPowerRange()
	{
		return Settings->TransmitPowerMax - Settings->TransmitPowerMin;
	}

	uint8_t GetTransmitPower()
	{
		return Settings->TransmitPowerMin + (uint8_t)(((uint16_t)GetTransmitPowerNormalized() * GetTransmitPowerRange()) / UINT8_MAX);
	}

	uint8_t GetTransmitPowerNormalized()
	{
		return CurrentTransmitPower;
	}

	void SetMaxPower()
	{
		CurrentTransmitPower = RADIO_POWER_BALANCER_POWER_MAX;
		LoLaDriver->InvalidatedTransmitPower();
	}

	void OnDisable()
	{
		SetMaxPower();
	}

	bool Callback()
	{
		if (!LinkInfo->HasLink())
		{
			Task::disable();
			return false;
		}
		else
		{
			if (Update())
			{
				LoLaDriver->InvalidatedTransmitPower();
			}

			return true;
		}
	}

private:
	// Returns true if update is needed.
	bool Update()
	{
		if (LastUpdated == 0 ||
			LinkInfo->GetElapsedMillisLastValidReceived() >= LOLA_LINK_SERVICE_LINKED_MAX_PANIC)
		{
			LastUpdated = millis();
			CurrentTransmitPower = RADIO_POWER_BALANCER_POWER_MAX;

			return true;
		}
		else if (millis() - LastUpdated > LOLA_LINK_SERVICE_LINKED_POWER_UPDATE_PERIOD)
		{
			LastUpdated = millis();

			uint8_t CurrentPartnerRSSI = LinkInfo->GetPartnerRSSINormalized();

			if ((CurrentPartnerRSSI > RADIO_POWER_BALANCER_RSSI_TARGET) &&
				(CurrentTransmitPower > RADIO_POWER_BALANCER_POWER_MIN))
			{
				// Down.
				if (CurrentPartnerRSSI > (RADIO_POWER_BALANCER_RSSI_TARGET + RADIO_POWER_BALANCER_RSSI_TARGET_MARGIN))
				{
					CurrentTransmitPower = max((uint8_t)RADIO_POWER_BALANCER_POWER_MIN, (uint8_t)(CurrentTransmitPower - RADIO_POWER_BALANCER_ADJUST_DOWN_LONG));
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
					CurrentTransmitPower = min((uint16_t)RADIO_POWER_BALANCER_POWER_MAX, CurrentTransmitPower + RADIO_POWER_BALANCER_ADJUST_UP_LONG);

				}
				else if (CurrentTransmitPower < RADIO_POWER_BALANCER_POWER_MAX)
				{
					CurrentTransmitPower++;
				}
			}

			return true;
		}

		return false;
	}
};
#endif

