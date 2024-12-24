// LinkClock.h

#ifndef _LINK_CLOCK_h
#define _LINK_CLOCK_h

#include "TuneClock.h"

/// <summary>
/// Tuneable real time clock.
/// Rolls over after ~136.1 years.
/// </summary>
class LinkClock final : public TuneClock
{
public:
	LinkClock(TS::Scheduler& scheduler, ICycles* cycles)
		: TuneClock(scheduler, cycles)
	{}
};
#endif