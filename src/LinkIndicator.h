// LinkIndicator.h

#ifndef _LINKINDICATOR_h
#define _LINKINDICATOR_h

#include <Callback.h>

class ILinkIndicator
{
public:
	virtual bool HasLink()
	{
		return false;
	}
};

class LinkStatus : public ILinkIndicator
{
public:
	enum StateEnum : uint8_t
	{
		Disabled = 0,
		Setup = 1,
		AwaitingLink = 2,
		AwaitingSleeping = 3,
		Linking = 4,
		Linked = 5
	};

private:
	//Callback handler.
	static const uint8_t MaxSlots = 16;
	Signal<const LinkStatus::StateEnum, MaxSlots> LinkStatusUpdated;

	StateEnum LinkState = StateEnum::Disabled;

public:
	LinkStatus()
	{
	}

	StateEnum GetLinkState()
	{
		return LinkState;
	}

	void UpdateState(const LinkStatus::StateEnum newState)
	{
		if (LinkState != newState)
		{
			LinkState = newState;
			LinkStatusUpdated.fire(LinkState);
		}	
	}

	void AttachOnLinkStatusUpdated(const Slot<const LinkStatus::StateEnum>& slot)
	{
		LinkStatusUpdated.attach(slot);
	}

	bool IsDisabled()
	{
		return LinkState == StateEnum::Disabled;
	}

	bool HasLink()
	{
		return LinkState == StateEnum::Linked;
	}
};
#endif

