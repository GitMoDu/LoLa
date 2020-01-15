// LoLaLinkChannelManager.h

#ifndef _LOLA_LINK_CHANNEL_MANAGER_h
#define _LOLA_LINK_CHANNEL_MANAGER_h

#include <Services\Link\LoLaLinkDefinitions.h>
#include <LoLaClock\IClock.h>
#include <FastCRC.h>

#include <Services\Link\LoLaLinkTimedHopper.h>



class LoLaLinkChannelManager : public virtual IChannelSelector
{
private:
	const uint32_t MinPeriodMillis = 5;
	const uint32_t MaxPeriodMillis = 60000;

	// Protocol defined values
	static const uint8_t SeedSize = LOLA_LINK_CHANNEL_SEED_SIZE;
	static const uint8_t HopPeriodMillis = LOLA_LINK_SERVICE_LINKED_TOKEN_HOP_PERIOD_MILLIS;

	ITokenSource* TokenSource = nullptr;

	struct ChannelsInfoStruct
	{
		uint8_t Min = 0;
		uint8_t Max = 0;
		uint8_t Range = 0;
	} ChannelInfo;

	bool SyncedMode = false;

	uint8_t CurrentChannel = 0;

	// Pseudo-random generator, for channel spreading.
	// Maybe just a Modulus(TOKEN , UINT8_MAX) does the job just as well.
	FastCRC8 FastHasher;

	struct EntropyStruct
	{
		uint8_t array[sizeof(uint32_t)];
		uint32_t uint;
	} Entropy;

	uint32_t LastToken = UINT32_MAX;

	// Channel hop updater.
	LoLaLinkTimedHopper TimedHopper;

public:
	LoLaLinkChannelManager(Scheduler* scheduler) : IChannelSelector()
		, FastHasher()
		, TimedHopper(scheduler)
	{
	}

	void NextRandomChannel()
	{
		CurrentChannel = ChannelInfo.Min + (random(INT32_MAX) % ChannelInfo.Range);
	}

	void NextChannel()
	{
		CurrentChannel++;

		if (CurrentChannel > ChannelInfo.Max)
		{
			CurrentChannel = ChannelInfo.Min + ((CurrentChannel - 1) % ChannelInfo.Max);
		}
	}

	void SetSyncedMode(const bool enabled)
	{
		SyncedMode = enabled;
	}

	virtual uint8_t GetChannel()
	{
#ifdef LOLA_LINK_USE_CHANNEL_HOP
		if (SyncedMode)
		{
			Entropy.uint = TokenSource->GetToken();

			if (Entropy.uint != LastToken)
			{
				LastToken = Entropy.uint;

				CurrentChannel = ChannelInfo.Min +
					(FastHasher.smbus((uint8_t*)&Entropy, sizeof(Entropy)) % ChannelInfo.Range);
			}
		}
#endif

		return CurrentChannel;
	}

	bool Setup(ILoLaDriver* lolaDriver, ITokenSource* tokenSource, ISyncedClock* syncedClock)
	{
		if (HopPeriodMillis <= MaxPeriodMillis &&
			HopPeriodMillis > MinPeriodMillis&&
			lolaDriver != nullptr &&
			tokenSource != nullptr &&
			TimedHopper.Setup(syncedClock))
		{
			TokenSource = tokenSource;

			ChannelInfo.Min = lolaDriver->GetChannelMin();
			ChannelInfo.Max = lolaDriver->GetChannelMax();
			ChannelInfo.Range = ChannelInfo.Max - ChannelInfo.Min;

			return true;
		}

		return false;
	}
};
#endif