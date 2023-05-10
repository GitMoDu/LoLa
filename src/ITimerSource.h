// ITimerSource.h
#ifndef _I_TIMER_SOURCE_h
#define _I_TIMER_SOURCE_h

#include <stdint.h>

class ITimerSource
{
public:
	///// <summary>
	///// Timer Source should provide a rolling counter, with <2 us steps.
	///// 8 bit composite timers are vulnerable to interrupt hogging, but work (i.e. AVR Micros).
	///// 16 bit timers are adequate but low resolution.
	///// 16 bit composite timers are adequate for ~1 us steps.
	///// 32 bit timers are ideal with <1 us steps and/or longer overflow duration. 
	///// </summary>
	virtual const uint32_t GetCounter() { return 0; }

	virtual const uint32_t GetDefaultRollover() { return 0; }

	/// <summary>
	/// Start the timer source.
	/// </summary>
	virtual void StartTimer() { }

	/// <summary>
	/// Stop the timer source.
	/// </summary>
	virtual void StopTimer() {}
};
#endif