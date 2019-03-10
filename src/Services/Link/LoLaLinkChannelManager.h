// LoLaLinkChannelManager.h

#ifndef _LOLA_LINK_CHANNEL_MANAGER_h
#define _LOLA_LINK_CHANNEL_MANAGER_h

#include <ILoLa.h>
#include <Services\Link\LoLaLinkDefinitions.h>

class LoLaLinkChannelManager
{
private:
	LoLaLinkInfo* LinkInfo = nullptr;
	ILoLa* LoLa = nullptr;

	//uint32_t HopPeriod = LOLA_LINK_FREQUENCY_HOP_PERIOD_MILLIS;

	//Helpers
	//uint8_t HopIndex = 0;
	TinyCrcModbus8 Hasher;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} HopSeed;

	uint32_t HopIterator;

private:


	//bool UpdateLinkedChannel()
	//{
	//	return false;
	//}

	//uint8_t GetHopChannel()
	//{
	//	HopSeed.uint = (MillisSync() / HopPeriod);

	//	//We could get the channel from a table, since we have an index.
	//	//HopIndex = HopSeed.uint % HopCount;

	//	//But better yet, we can use the synced clock and crypto seed
	//	// to generate a pseudo-random channel distribution.
	//	Hasher.Reset(TokenCachedHash);
	//	Hasher.Update(HopSeed.array, 4);

	//	return (Hasher.GetCurrent() % (LoLa->GetChannelMax() + 1 - LoLa->GetChannelMin()))
	//		+ LoLa->GetChannelMin();
	//}

public:
	bool Setup(ILoLa* loLa)
	{
		LoLa = loLa;

		return LoLa != nullptr;
	}

	void ResetChannel()
	{
		LoLa->SetChannel((LoLa->GetChannelMin() + LoLa->GetChannelMax()) / 2);
	}

	bool Setup(ILoLa* loLa, LoLaLinkInfo* linkInfo)
	{
		LoLa = loLa;
		LinkInfo = linkInfo;

		return LoLa != nullptr && LinkInfo != nullptr;
	}
};
#endif

