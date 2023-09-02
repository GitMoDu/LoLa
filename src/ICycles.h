// ICycles.h
#ifndef _I_CYCLE_SOURCE_h
#define _I_CYCLE_SOURCE_h

#include <stdint.h>

static constexpr uint32_t ONE_SECOND_MICROS = 1000000;
static constexpr uint32_t ONE_SECOND_MILLIS = 1000;

static constexpr uint32_t ONE_MILLI_MICROS = 1000;

/// <summary>
/// Interface for system cycle count source.
/// </summary>
class ICycles
{
public:
	/// <summary>
	/// 
	/// </summary>
	/// <returns>Current cycle count.</returns>
	virtual const uint32_t GetCycles() { return 0; }

	/// <summary>
	/// Fixed value.
	/// </summary>
	/// <returns>How many cycles until an overflow occurs.</returns>
	virtual const uint32_t GetCyclesOverflow() { return 0; }

	/// <summary>
	/// Fixed value.
	/// </summary>
	/// <returns>How many cycles in one second.</returns>
	virtual const uint32_t GetCyclesOneSecond() { return 0; }

	/// <summary>
	/// Start the timer source.
	/// </summary>
	virtual void StartCycles() { }

	/// <summary>
	/// Stop the timer source.
	/// </summary>
	virtual void StopCycles() {}
};
#endif