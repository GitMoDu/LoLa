// LoLaLinkIndicator.h

#ifndef _LOLA_LINK_INDICATOR_h
#define _LOLA_LINK_INDICATOR_h

#include <stdint.h>
#include <Callback.h>

class LoLaLinkIndicator
{
private:
	// Link Indicator Callback handler.
	static const uint8_t MaxSlots = 16;
	Signal<const bool, MaxSlots> LinkStatusUpdated;

	bool LinkState = false;
	//

public:
	LoLaLinkIndicator() {}

	bool HasLink()
	{
		return LinkState;
	}

	bool UpdateLinkStatus(const bool linked)
	{
		if (LinkState != linked)
		{
			LinkState = linked;
			LinkStatusUpdated.fire(LinkState);

			return true;
		}

		return false;
	}

	void AttachOnLinkStatusUpdated(const Slot<const bool>& slot)
	{
		LinkStatusUpdated.attach(slot);
	}
};
#endif