// ArduinoCycles.h
#ifndef _ARDUINO_CYCLES_h
#define _ARDUINO_CYCLES_h

#include "ICycles.h"

/// <summary>
/// Uses Arduino micros() as a system cycle counter.
/// </summary>
#if defined(LOLA_UNIT_TESTING)
/// <typeparam name="TestOffset">Starting offset in us.</typeparam>
template<const uint32_t TestOffset = 0>
#endif
class ArduinoCycles final : public virtual ICycles
{
public:
	ArduinoCycles() : ICycles()
	{}

public:
	/// <summary>
	/// Arduino micros().
	/// </summary>
	/// <returns>Current cycle count.</returns>
	virtual const uint32_t GetCycles() final
	{
#if defined(LOLA_UNIT_TESTING)
		return micros() + TestOffset;
#else
		return micros();
#endif
	}

	/// <summary>
	/// </summary>
	/// <returns>How many cycles until an overflow occurs.</returns>
	virtual const uint32_t GetCyclesOverflow() final
	{
		return UINT32_MAX;
	}

	/// <summary>
	/// </summary>
	/// <returns>How many cycles in one second.</returns>
	virtual const uint32_t GetCyclesOneSecond() final
	{
		return ONE_SECOND_MICROS;
	}

	/// <summary>
	/// Arduino micros() is always running.
	/// Start does nothing.
	/// </summary>
	virtual void StartCycles() final
	{}

	/// <summary>
	/// Arduino micros() is always running.
	/// Stop does nothing.
	/// </summary>
	virtual void StopCycles() final
	{}
};
#endif