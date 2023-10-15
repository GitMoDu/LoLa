// LinkClock.h

#ifndef _LINK_CLOCK_h
#define _LINK_CLOCK_h

#include "TuneClock.h"
#include <ITimestamp.h>

/// <summary>
/// Tuneable real time clock.
/// Rolls over after ~136.1 years.
/// </summary>
class LinkClock final : public TuneClock, public virtual IRollingTimestamp
{
public:
	LinkClock(Scheduler& scheduler, ICycles* cycles)
		: TuneClock(scheduler, cycles)
	{}

public:
	virtual const uint32_t GetRollingTimestamp() final
	{
		Timestamp rollingTimestamp{};

		GetTimestamp(rollingTimestamp);

		return rollingTimestamp.GetRollingMicros();
	}
};
#endif