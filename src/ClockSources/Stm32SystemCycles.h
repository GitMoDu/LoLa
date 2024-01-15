// Stm32SystemCycles.h
#ifndef _STM32_SYSTEM_CLOCK_CYCLES_h
#define _STM32_SYSTEM_CLOCK_CYCLES_h

#include <ICycles.h>

/// <summary>
/// Uses STM32 System Clock Tick as the cycle counter.
/// </summary>
#if defined(LOLA_UNIT_TESTING)
/// <typeparam name="TestOffset">Starting offset in us.</typeparam>
template<const uint32_t TestOffset = 0>
#endif
class Stm32SystemCycles final : public virtual ICycles
{
private:
	static constexpr uint8_t COUNTER_SCALE = 2;

public:
	Stm32SystemCycles() : ICycles()
	{}

public:
	/// <summary>
	/// System Clock cycles counter.
	/// Value is scaled by COUNTER_SCALE.
	/// </summary>
	/// <returns>Current scaled cycle count.</returns>
	virtual const uint32_t GetCycles() final
	{
#if defined(LOLA_UNIT_TESTING)
		return GetCycleCount() + TestOffset;
#else
		return GetCycleCount();
#endif
	}

	/// <summary>
	/// </summary>
	/// <returns>How many cycles until an overflow occurs.</returns>
	virtual const uint32_t GetCyclesOverflow() final
	{
		return UINT32_MAX / COUNTER_SCALE;
	}

	/// <summary>
	/// </summary>
	/// <returns>How many cycles in one second.</returns>
	virtual const uint32_t GetCyclesOneSecond() final
	{
		return F_CPU / COUNTER_SCALE;
	}

	/// <summary>
	/// Systick is always running under Arduino core.
	/// Start does nothing.
	/// </summary>
	virtual void StartCycles() final
	{}

	/// <summary>
	/// Systick is always running under Arduino core.
	/// Stop does nothing.
	/// </summary>
	virtual void StopCycles() final
	{}

private:
	/// <summary>
	/// Following reference micros() implementation, 
	/// but returns COUNTER_SCALE factored counter instead of microseconds.
	/// </summary>
	/// <returns>Current Cycles [0;UINT32_MAX].</returns>
	const uint32_t GetCycleCount()
	{
		uint32 ms;
		uint32 cycle_cnt;

		do {
			ms = systick_uptime();
			cycle_cnt = SYSTICK_BASE->CNT;
			asm volatile("nop"); //allow interrupt to fire
			asm volatile("nop");
		} while (ms != systick_uptime());

		return ((uint32_t)(ms * (ONE_MILLI_MICROS / COUNTER_SCALE) * CYCLES_PER_MICROSECOND) + ((SYSTICK_RELOAD_VAL + 1 - cycle_cnt)) / COUNTER_SCALE);
	}
};
#endif