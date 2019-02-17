// LoLaLinkFrequencyHopper.h

#ifndef _LOLAFREQUENCYHOPPER_h
#define _LOLAFREQUENCYHOPPER_h

#include <Crypto\TinyCRC.h>
#include <Services\ILoLaService.h>
#include <ILoLa.h>
#include <Crypto\ISeedSource.h>

#define LOLA_LINK_FREQUENCY_HOPPER_WARMUP_MILLIS 10

class LoLaLinkFrequencyHopper : public ILoLaService
{
private:
	///Setup time constants.
	uint8_t ChannelDefault = 0;
	uint8_t ChannelMin = 0;
	uint8_t ChannelMax = 0;
	///

	///Session constants.
	uint32_t HopPeriod = LOLA_LINK_FREQUENCY_HOP_PERIOD_MILLIS;
	uint8_t TokenCachedHash = 0;
	bool IsWarmUpTime = false;
	///

	///Crypto Token generator.
	ISeedSource* CryptoSeed = nullptr;
	///

	//Helpers
	//uint8_t HopIndex = 0;
	TinyCrcModbus8 Hasher;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} HopSeed;

	uint32_t HopIterator;

public:
	LoLaLinkFrequencyHopper(Scheduler* scheduler, ILoLa* loLa)
		: ILoLaService(scheduler, 0, loLa)
	{

	}

	bool SetCryptoSeedSource(ISeedSource* cryptoSeedSource)
	{
		CryptoSeed = cryptoSeedSource;

		return CryptoSeed != nullptr;
	}

	void ResetChannel()
	{
		GetLoLa()->SetChannel(ChannelDefault);
		GetLoLa()->OnStart();
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("LinkFrequencyHopper"));
	}
#endif // DEBUG_LOLA

	void OnLinkEstablished()
	{
		if (IsSetupOk())
		{
			TokenCachedHash = CryptoSeed->GetCurrentSeed();
			Enable();
		}
	}

	void OnLinkLost()
	{
		ResetChannel();
		TokenCachedHash = CryptoSeed->GetCurrentSeed();
		Disable();
	}

	bool OnEnable()
	{
		IsWarmUpTime = true;
		return true;
	}

	bool Callback()
	{
		if (IsWarmUpTime)
		{
			SetNextRunDelay(LOLA_LINK_FREQUENCY_HOPPER_WARMUP_MILLIS);
			IsWarmUpTime = false;
			return true;
		}
		else
		{
			GetLoLa()->SetChannel(GetHopChannel());

			//Try to sync the next run based on the synced clock.
			SetNextRunDelay(GetNextSwitchOverDelay());
		}

		return true;
	}

	bool OnSetup()
	{
		ChannelMin = GetLoLa()->GetChannelMin();
		ChannelMax = GetLoLa()->GetChannelMax();

		ChannelDefault = (ChannelMin + ChannelMax) / 2;

		return true;
	}

private:
	uint32_t GetNextSwitchOverDelay()
	{
		HopIterator = 0;
		while (HopIterator <= HopPeriod)
		{
			//We search backwards because for most of the time, the first iteration will return a result.
			if ((MillisSync() + HopPeriod - HopIterator) % HopPeriod == 0)
			{
				return HopPeriod - HopIterator;
			}
			HopIterator++;
		}

		//This should never happen.
		return HopIterator;
	}

	uint8_t GetHopChannel()
	{
		HopSeed.uint = (MillisSync() / HopPeriod);

		//We could get the channel from a table, since we have an index.
		//HopIndex = HopSeed.uint % HopCount;

		//But better yet, we can use the synced clock and crypto seed
		// to generate a pseudo-random channel distribution.
		Hasher.Reset(TokenCachedHash);
		Hasher.Update(HopSeed.array, 4);

		return (Hasher.GetCurrent() % (ChannelMax + 1 - ChannelMin)) + ChannelMin;
	}
};
#endif