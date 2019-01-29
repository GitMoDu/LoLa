// LoLaFrequencyHopper.h

#ifndef _LOLAFREQUENCYHOPPER_h
#define _LOLAFREQUENCYHOPPER_h

#include <Crypto\TinyCRC.h>
#include <Services\ILoLaService.h>

//Hop period should be a multiple of Send Slot Period.
#define LOLA_FREQUENCY_HOPPER_HOP_PERIOD_MILLIS	(10*ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS) //Max 256 ms of hop period.

//#define LOLA_FREQUENCY_HOPPER_HOP_COUNT			200 //Max 256 hop count.

class LoLaFrequencyHopper : public ILoLaService
{
private:
	bool LinkCallbackSet = false;

	//Compile time constants.
	uint8_t ChannelDefault = 0;
	uint8_t ChannelMin = 0;
	uint8_t ChannelMax = 0;

	uint8_t HopPeriod = LOLA_FREQUENCY_HOPPER_HOP_PERIOD_MILLIS;

	//Session constants.
	uint8_t TokenCachedHash = 0;

	//Helpers
	//uint8_t HopIndex = 0;
	TinyCrcModbus8 Hasher;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} HopSeed;


public:
	LoLaFrequencyHopper(Scheduler* scheduler, ILoLa* loLa)
		: ILoLaService(scheduler, 0, loLa)
	{
		ChannelMin = lola->GetChannelMin();
		ChannelMax = lola->GetChannelMax();

		ChannelDefault = (ChannelMin + ChannelMax) / 2;
	}

	void OnLinkStatusUpdated(const uint8_t state)
	{
		if (!IsSetupOk())
		{
			return;
		}

		if (state == LoLaLinkInfo::LinkStateEnum::Connected)
		{
			Enable();
		}
		else
		{
			ResetChannel();
			Disable();
		}
	}

	bool OnEnable()
	{
		SetNextRunASAP();

		return true;
	}

	bool SetLink(LoLaLinkService * linkService)
	{
		if (linkService != null)
		{
			FunctionSlot<uint8_t> funcSlot(OnLinkStatusUpdated);
			linkService->AttachOnLinkStatusUpdated(funcSlot);
		}
		return LinkCallbackSet;
	}

	bool Callback()
	{
		GetLoLa()->SetChannel(GetHopChannel(MillisSync()));

		//Instead of relying purely on the scheduler, we adapt to any timming mis-sync on every hop.
		SetNextRunDelay(GetNextSwitchOverDelay(MillisSync()));

		return true;
	}

	bool OnSetup()
	{
		return true;
	}

private:
	void ResetChannel()
	{
		GetLoLa()->SetChannel(ChannelDefault);
	}

	uint8_t GetNextSwitchOverDelay(const uint32_t millisSync)
	{
		for (uint8_t i = 0; i < HopPeriod; i++)
		{
			//We search backwards because for most of the time, the first iteration will return a result.
			if ((millisSync + HopPeriod - i) % HopPeriod == 0)
			{
				return HopPeriod - i;
			}
		}

		Serial.println("Error GetNextSwitchOverDelay");
		ResetChannel();
		Disable();

		return 0;
	}

	uint8_t GetHopChannel(const uint32_t millisSync)
	{
		HopSeed.uint = (millisSync / HopPeriod);

		//We can get the channel from a table, since we have an index.
		//HopIndex = HopSeed.uint % HopCount;

		//Ooooooor, we can use the synced seed as a common source for a pseudo-random distribution.
		Hasher.Reset(TokenCachedHash);

		Hasher.Update32(HopSeed);

		return map(Hasher.GetCurrent(), 0, UINT8_MAX, ChannelMin, ChannelMax);
	}
};
#endif