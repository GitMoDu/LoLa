// CycleCounter.h

#ifndef _TASK_CYCLE_COUNTER_h
#define _TASK_CYCLE_COUNTER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ICycles.h>

/// <summary>
/// Task based Cycle Counter.
/// Abstracts Cycles to UINT32_MAX overflow counter, with precision <= 1us. 
/// Uses task callback to ensure cycles' overflows are tracked.
/// Inheritable RunCheck for extra checks using the same task and latest cyclestamp.
/// </summary>
class CycleCounter : protected Task
{
private:
	static constexpr uint32_t MIN_OVERFLOW_COUNT = UINT16_MAX - 1;
	static constexpr uint32_t MAX_SECONDS_COUNT = ((uint32_t)1000 * ONE_MILLI_MICROS);
	static constexpr uint16_t MIN_ROLLOVER_PERIOD = INT16_MAX;

private:
	ICycles* CyclesSource;

private:
	uint32_t CyclesSecond = ONE_SECOND_MICROS;
	uint16_t CyclesMilli = ONE_MILLI_MICROS;

	uint32_t CyclesOverflow = UINT32_MAX;
	uint32_t RolloverPeriod = UINT32_MAX;

private:
	uint32_t LastCycles = 0;
	uint16_t Overflows = 0;

protected:
	/// <summary>
	/// RunCheck for extra checks using the same task callback.
	/// </summary>
	/// <param name="cyclestamp">The current pass's cyclestamp.</param>
	/// <returns>Max delay in milliseconds until next run is required.</returns>
	virtual uint32_t RunCheck(const uint32_t cyclestamp) { return UINT32_MAX; }

public:
	CycleCounter(Scheduler& scheduler, ICycles* cycles)
		: Task(0, TASK_FOREVER, &scheduler, false)
		, CyclesSource(cycles)
	{}

	virtual const bool Setup()
	{
		if (CyclesSource != nullptr)
		{
			CyclesOverflow = CyclesSource->GetCyclesOverflow();
			if (CyclesOverflow < MIN_OVERFLOW_COUNT)
			{
				return false;
			}

			CyclesSecond = CyclesSource->GetCyclesOneSecond();
			if ((CyclesSecond < ONE_SECOND_MICROS)
				|| ((CyclesSecond / ONE_MILLI_MICROS) > MAX_SECONDS_COUNT))
			{
				return false;
			}

			CyclesMilli = CyclesSecond / ONE_SECOND_MILLIS;

			RolloverPeriod = (((uint64_t)CyclesOverflow) * ONE_SECOND_MICROS) / CyclesSecond;

			return RolloverPeriod >= MIN_ROLLOVER_PERIOD;
		}

		return false;
	}

	void Stop()
	{
		CyclesSource->StopCycles();
		Task::disable();
	}

public:
	/// <summary>
	/// Task callback override.
	/// Executes all checks and schedules next for the shortest one.
	/// </summary>
	virtual bool Callback() final
	{
		const uint32_t cycles = CyclesSource->GetCycles();

		const uint32_t overflowDelay = CheckOverflows(cycles);

		const uint32_t checkDelay = RunCheck(GetCyclestampInternal(cycles));

		if (overflowDelay <= checkDelay)
		{
			if (overflowDelay > 0)
			{
				Task::delay(overflowDelay);
			}
			else
			{
				Task::enable();
			}
		}
		else if (checkDelay > 0)
		{
			Task::delay(checkDelay);
		}
		else
		{
			Task::enable();
		}

		return true;
	}

	/// <summary>
	/// Monotonic, overflow abstracted cycle count.
	/// Can be used for durations up RolloverPeriod (us).
	/// </summary>
	/// <returns>Internal overflow aware cycle count [0; UINT32_MAX]</returns>
	const uint32_t GetCyclestamp()
	{
		if (CyclesOverflow == UINT32_MAX)
		{
			return CyclesSource->GetCycles();
		}
		else
		{
			return GetCyclestamp(CyclesSource->GetCycles());
		}
	}

	/// <summary>
	/// Convert a recent raw cycle count into a cyclestamp.
	/// Cyclestamp be used for durations up RolloverPeriod (us).
	/// </summary>
	/// <param name="cycles">Raw cycle count [0; CyclesOverflow]</param>
	/// <returns>Internal overflow aware cycle count [0; UINT32_MAX]</returns>
	const uint32_t GetCyclestamp(const uint32_t cycles)
	{
		if (CyclesOverflow == UINT32_MAX)
		{
			return cycles;
		}
		else
		{
			CheckOverflowsInternal(cycles);

			return (CyclesOverflow * Overflows) + cycles;
		}
	}

	/// <summary>
	/// Elapsed duration since counter started.
	/// </summary>
	/// <param name="cyclestamp"></param>
	/// <returns></returns>
	const uint32_t GetElapsedDuration(const uint32_t cyclestamp)
	{
		if (CyclesMilli == ONE_MILLI_MICROS)
		{
			return cyclestamp;
		}
		else if (CyclesOverflow <= UINT16_MAX)
		{
			return (((uint32_t)(cyclestamp)) * ONE_MILLI_MICROS) / CyclesMilli;
		}
		else
		{
			return (((uint64_t)(cyclestamp)) * ONE_MILLI_MICROS) / CyclesMilli;
		}
	}

protected:
	/// <summary>
	/// Start clock and cycle source, if not yet started.
	/// </summary>
	virtual void Start()
	{
		if (!Task::isEnabled())
		{
			CyclesSource->StartCycles();
			LastCycles = 0;

		}
		Task::enable();
	}

	/// <summary>
	/// Get duration from cyclestamp counts.
	/// </summary>
	/// <param name="cyclestampStart"></param>
	/// <param name="cyclestampEnd"></param>
	/// <returns>[0;UINT32_MAX] us</returns>
	const uint32_t GetDurationCyclestamp(const uint32_t cyclestampStart, const uint32_t cyclestampEnd)
	{
		if (CyclesMilli == ONE_MILLI_MICROS)
		{
			if (cyclestampEnd >= cyclestampStart)
			{
				return cyclestampEnd - cyclestampStart;
			}
			else
			{
				return (UINT32_MAX - cyclestampStart) + cyclestampEnd;
			}
		}
		else
		{
			if (cyclestampEnd >= cyclestampStart)
			{
				return (((uint64_t)(cyclestampEnd - cyclestampStart) * ONE_MILLI_MICROS) / CyclesMilli);
			}
			else
			{
				return (((uint64_t)((UINT32_MAX - cyclestampStart) + cyclestampEnd) * ONE_MILLI_MICROS) / CyclesMilli);
			}
		}
	}

private:
	/// <summary>
	/// Assumes durations smaller than RolloverPeriod.
	/// </summary>
	/// <param name="cyclesStart"></param>
	/// <param name="cyclesEnd"></param>
	/// <returns>Duration in us.</returns>
	const uint32_t GetDurationCycles(const uint32_t cyclesStart, const uint32_t cyclesEnd)
	{
		if (CyclesMilli == ONE_MILLI_MICROS)
		{
			if (cyclesEnd >= cyclesStart)
			{
				return cyclesEnd - cyclesStart;
			}
			else
			{
				return (CyclesOverflow - cyclesStart) + cyclesEnd;
			}
		}
		else if (CyclesOverflow <= UINT16_MAX)
		{
			if (cyclesEnd >= cyclesStart)
			{
				return ((cyclesEnd - cyclesStart) * ONE_MILLI_MICROS) / CyclesMilli;
			}
			else
			{
				return (((CyclesOverflow - cyclesStart) + cyclesEnd) * ONE_MILLI_MICROS) / CyclesMilli;
			}
		}
		else
		{
			if (cyclesEnd >= cyclesStart)
			{
				return ((((uint64_t)cyclesEnd - cyclesStart) * ONE_MILLI_MICROS) / CyclesMilli);
			}
			else
			{
				return (((((uint64_t)CyclesOverflow - cyclesStart) + cyclesEnd) * ONE_MILLI_MICROS) / CyclesMilli);
			}
		}
	}

	/// <summary>
	/// </summary>
	/// <param name="cycles"></param>
	/// <returns>Delay until next sleep in ms.</returns>
	const uint32_t CheckOverflows(const uint32_t cycles)
	{
		if (CyclesOverflow != UINT32_MAX)
		{
			if (CheckOverflowsInternal(cycles))
			{
				return 0;
			}
			else
			{
				const uint32_t nextOverflowDelay = GetDurationCycles(cycles, CyclesOverflow);

				if (nextOverflowDelay <= ONE_SECOND_MICROS)
				{
					if (nextOverflowDelay >= ONE_MILLI_MICROS)
					{
						// Sleep until we're one milisecond away from overflow.
						return (nextOverflowDelay - ONE_MILLI_MICROS) / ONE_MILLI_MICROS;
					}
					else
					{
						// Rollover is imminent.
						return 0;
					}
				}
				else
				{
					// Sleep until next second and one milisecond away from overflow.
					return ONE_SECOND_MICROS - ONE_MILLI_MICROS;
				}
			}
		}
		else
		{
			return UINT32_MAX;
		}
	}

	const bool CheckOverflowsInternal(const uint32_t cycles)
	{
		// Check cycles for rollover and compensate in overflows.
		if (cycles < LastCycles)
		{
			Overflows++;
			LastCycles = cycles;

			return true;
		}
		else
		{
			LastCycles = cycles;

			return false;
		}
	}

	/// <summary>
	/// Optimized version with no CheckOverflows.
	/// </summary>
	/// <param name="cycles">Raw cycle count [0; CyclesOverflow]</param>
	/// <returns>Internal overflow aware cycle count [0; UINT32_MAX]</returns>
	const uint32_t GetCyclestampInternal(const uint32_t cycles)
	{
		if (CyclesOverflow < UINT32_MAX)
		{
			return ((CyclesOverflow + 1) * Overflows) + cycles;
		}
		else
		{
			return cycles;
		}
	}
};
#endif