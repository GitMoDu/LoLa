// LoLaLinkChannelManager.h

#ifndef _LOLA_LINK_CHANNEL_MANAGER_h
#define _LOLA_LINK_CHANNEL_MANAGER_h

#include <ILoLaDriver.h>
#include <Services\Link\LoLaLinkDefinitions.h>

class LoLaLinkChannelManager
{
private:
	LoLaLinkInfo* LinkInfo = nullptr;
	ILoLaDriver* LoLaDriver = nullptr;

public:
	void ResetChannel()
	{
		LoLaDriver->SetChannel((LoLaDriver->GetChannelMin() + LoLaDriver->GetChannelMax()) / 2);
	}

	//Hop index from 0 to UINT8_MAX
	void SetNextHop(const uint8_t hopIndex)
	{
		LoLaDriver->SetChannel((hopIndex % (LoLaDriver->GetChannelMax() + 1 - LoLaDriver->GetChannelMin()))
			+ LoLaDriver->GetChannelMin());

#if defined(DEBUG_LOLA) && defined(DEBUG_LINK_FREQUENCY_HOP)
		Serial.print(F("Hop: "));
		Serial.println(LoLaDriver->GetChannel());
#endif
	}

	bool Setup(ILoLaDriver* loLa)
	{
		LoLaDriver = loLa;

		return LoLaDriver != nullptr;
	}
};
#endif

