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

	//Hop index from 0 to UINT8_MAX
	void SetNextHop(const uint8_t hopIndex)
	{
		LoLa->SetChannel((hopIndex % (LoLa->GetChannelMax() + 1 - LoLa->GetChannelMin()))
			+ LoLa->GetChannelMin());
	}

	bool Setup(ILoLa* loLa, LoLaLinkInfo* linkInfo)
	{
		LoLa = loLa;
		LinkInfo = linkInfo;

		return LoLa != nullptr && LinkInfo != nullptr;
	}
};
#endif

