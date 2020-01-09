// TickTrainedTimerSyncedClock.h

#ifndef _TRAINED_TIMER_SYNCED_CLOCK_h
#define _TRAINED_TIMER_SYNCED_CLOCK_h

#include <LoLaClock\LoLaSyncedClock.h>
#include <LoLaClock\TrainableTimerTimestampSource.h>
#include <LoLaClock\ClockTicker.h>

class TickTrainedTimerSyncedClock : public LoLaSyncedClock
{
private:
	TickTrainedTimerSyncedClockWithCallback TimerSource;
	ClockTicker Ticker;

public:
	TickTrainedTimerSyncedClock()
		: LoLaSyncedClock()
		, Ticker()
		, TimerSource()
	{
		TickSource = &Ticker;
		TimestampSource = &TimerSource;
	}

	virtual void OnTrainingTick()
	{
		OnTick(TimerSource.OnTrainingTick());
	}

	bool Setup()
	{
		if (!LoLaSyncedClock::Setup())
		{
			return false;
		}

		if (Ticker.Setup(this) &&
			TimerSource.Setup(&Ticker))
		{
			return true;
		}

		return false;
	}
};
#endif

