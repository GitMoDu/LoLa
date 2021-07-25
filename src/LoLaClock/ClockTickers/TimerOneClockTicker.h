// TimerOneClockTicker.h

#ifndef _STM32_RTC_CLOCK_TICKER_h
#define _STM32_RTC_CLOCK_TICKER_h

#include <LoLaClock\TickTrainedTimerSyncedClock.h>
#include <TimerOne.h>

class ClockTicker : public virtual TickTrainedTimerSyncedClock
{
private:


public:
	ClockTicker() : TickTrainedTimerSyncedClock()
	{
	}
};
#endif