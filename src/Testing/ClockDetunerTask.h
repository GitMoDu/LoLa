// ClockDetunerTask.h

#ifndef _CLOCK_DETUNERTASK_h
#define _CLOCK_DETUNERTASK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "../../LoLa/src/Clock/TuneClock.h"

class ClockDetunerTask : private Task
{
private:
	TuneClock* Clock = nullptr;

	uint32_t LastSmear = 0;
	int16_t DetuneMicros = 0;

public:
	ClockDetunerTask(Scheduler& scheduler)
		: Task(0, TASK_FOREVER, &scheduler, false)
	{}

	void SetClockDetune(TuneClock* clock, const int16_t detuneMicros)
	{
		Clock = clock;
		DetuneMicros = detuneMicros;

		if (Clock != nullptr)
		{
			Task::enable();
		}
		else
		{
			Task::disable();
		}
	}

	virtual bool Callback() final
	{
		const uint32_t smearDelay = CheckSmearAdaptive(micros());

		Task::delay(smearDelay);

		return true;
	}

private:
	const uint32_t CheckSmearAdaptive(const uint32_t timestamp)
	{
		const uint32_t elapsedSinceLastSmear = (timestamp - LastSmear) / ONE_MILLI_MICROS;
		const uint32_t smearPeriod = GetAdaptiveSmearPeriod();

		if (elapsedSinceLastSmear >= smearPeriod)
		{
			LastSmear = timestamp;

			// Smear up/down. according to tune.
			if (DetuneMicros > 0)
			{
				Clock->ShiftSubSeconds(+1);
			}
			else if (DetuneMicros < 0)
			{
				Clock->ShiftSubSeconds(-1);
			}

			return 0;
		}
		else
		{
			return smearPeriod - elapsedSinceLastSmear;
		}

		return true;
	}

	const uint32_t GetAdaptiveSmearPeriod() const
	{
		if (DetuneMicros > 0)
		{
			return ONE_SECOND_MILLIS / DetuneMicros;
		}
		else if (DetuneMicros < 0)
		{
			return ONE_SECOND_MILLIS / -DetuneMicros;
		}
		else
		{
			return ONE_SECOND_MILLIS;
		}
	}
};
#endif