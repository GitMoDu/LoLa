// Stm32TimerCycles.h
#ifndef _STM32_TIMER_CYCLES_h
#define _STM32_TIMER_CYCLES_h

#include <ICycles.h>
#include <Arduino.h>

/// <summary>
/// STM32 timer cycle counter.
/// </summary>
template<const uint8_t TimerIndex, const uint8_t TimerChannel>
class Stm32TimerCycles : public virtual ICycles
{
private:
	static constexpr uint32_t TICKS_ONE_SECOND = ONE_SECOND_MICROS;

public:
	Stm32TimerCycles() : ICycles()
	{}

public:
	/// <summary>
	/// 
	/// </summary>
	/// <returns>Current cycle count.</returns>
	virtual const uint32_t GetCycles() final
	{
		//TODO: return TimerInstance->count();
		return 0;
	}

	/// <summary>
	/// Fixed value.
	/// </summary>
	/// <returns>How many cycles until an overflow occurs.</returns>
	virtual const uint32_t GetCyclesOverflow() final
	{
		return UINT16_MAX;
	}

	/// <summary>
	/// </summary>
	/// <returns>How many cycles in one second.</returns>
	virtual const uint32_t GetCyclesOneSecond()  final
	{
		return TICKS_ONE_SECOND;
	}

	/// <summary>
	/// Start the timer source.
	/// </summary>
	virtual void StartCycles() final
	{
		//TODO: Start timer.
	}

	/// <summary>
	/// Stop the timer source.
	/// </summary>
	virtual void StopCycles() final
	{
		//TODO: Stop timer.
	}
};
#endif