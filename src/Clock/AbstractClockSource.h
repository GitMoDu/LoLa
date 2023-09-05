// AbstractClockSource.h

#ifndef _ABSTRACT_CLOCK_SOURCE_h
#define _ABSTRACT_CLOCK_SOURCE_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "..\IClockSource.h"


/// <summary>
/// </summary>
class AbstractClockSource : protected Task, public virtual IClockSource
{
private:
	IClockSource::IClockListener* TickListener = nullptr;

public:
	AbstractClockSource(Scheduler& scheduler)
		: IClockSource()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
	}

public:
	/// <summary>
	/// Start the clock source, 
	/// with tune parameter to speed up/slow down as requested.
	/// </summary>
	/// <param name="ppm">Absolute tune value, in us/s.</param>
	virtual const bool StartClock(IClockSource::IClockListener* tickListener, const int16_t ppm)
	{
		TickListener = tickListener;

		return TickListener != nullptr;
	}

	virtual void StopClock() {	}

	/// <summary>
	/// Arduino Clock Source is not tuneable.
	/// </summary>
	/// <param name="ppm"></param>
	virtual void TuneClock(const int16_t ppm) {}


public:
	void OnClockTick()
	{
		TickListener->OnClockTick();
	}
};
#endif