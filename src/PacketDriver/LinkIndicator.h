// LinkIndicator.h

#ifndef _LINKINDICATOR_h
#define _LINKINDICATOR_h

#include <Callback.h>

class LinkIndicator
{
private:
	//Callback handler.
	static const uint8_t MaxSlots = 16;
	Signal<const bool, MaxSlots> LinkStatusUpdated;

	bool LinkState = false;

public:


public:
	LinkIndicator()
	{
	}

	bool HasLink()
	{
		return LinkState;
	}

	bool UpdateState(const bool linked)
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

