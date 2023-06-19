// SynchronizedClock.h

#ifndef _SYNCHRONIZED_CLOCK_h
#define _SYNCHRONIZED_CLOCK_h

#include <ITimerSource.h>
#include "Timestamp.h"
#include "..\Clock\AbstractClockSource.h"

/// <summary>
/// Provides Seconds and Microseconds,
/// driven by a single 32 bit us timer.
/// Timer wrapping is handled automatically but is not monotonic.
/// </summary>
class SynchronizedClock : public virtual IClockSource::IClockListener
{
private:
	/// <summary>
	/// UINT32_MAX/1000000 us = 4294.967295 s
	/// </summary>
	static constexpr uint16_t MICROS_OVERFLOW_WRAP_SECONDS = 4294;
	static constexpr uint32_t MICROS_OVERFLOW_WRAP_REMAINDER = 967295;

private:
	int32_t OffsetMicros = 0;

	uint32_t LastTickCounter = 0;
	volatile uint32_t TickCounter = 0;
	volatile uint32_t CountsOneSecond = 0;
	volatile uint32_t Seconds = 0;

private:
	/// <summary>
	/// 32 bit, us clock source.
	/// </summary>
private:
	ITimerSource* TimerSource;
	IClockSource* ClockSource;

	const bool ClockHasTick;

public:
	SynchronizedClock(IClockSource* clockSource, ITimerSource* timerSource)
		: IClockSource::IClockListener()
		, TimerSource(timerSource)
		, ClockSource(clockSource)
		, ClockHasTick(ClockSource->ClockHasTick())
	{}

public:
	virtual void OnClockTick() final
	{
		// Get latest counter value before updating seconds.
		TickCounter = TimerSource->GetCounter();
		Seconds++;

		// Adjust counts for each tick.
		//TODO: Ajdust smoothly? Move to task? External call?
		CountsOneSecond = TickCounter - LastTickCounter;

		// Store this count for next cycle.
		LastTickCounter = TickCounter;
	}

public:
	void Start()
	{
		TimerSource->StartTimer();
		ClockSource->StartClock(this, 0);
	}

	void Stop()
	{
		TimerSource->StopTimer();
		ClockSource->StopClock();
	}

	const bool Setup()
	{
		if (ClockSource != nullptr && TimerSource != nullptr)
		{
			CountsOneSecond = TimerSource->GetDefaultRollover();
			return true;
		}

		return false;

	}
	void SetTuneOffset(const int16_t ppm)
	{
		ClockSource->TuneClock(ppm);
	}

	void ShiftSeconds(const int32_t offsetSeconds)
	{
		noInterrupts();
		Seconds += offsetSeconds;
		interrupts();
	}

	void GetTimestamp(Timestamp& timestamp)
	{
		// Stop interrupts and get the latest clock seconds and timer counter.
		noInterrupts();
		timestamp.SubSeconds = TimerSource->GetCounter();
		timestamp.Seconds = Seconds;
		interrupts();

		if (ClockHasTick)
		{
			if (timestamp.SubSeconds < TickCounter)
			{
				// Overflow detected and compensated before final subseconds calculation.
				timestamp.Seconds++;
				timestamp.SubSeconds += CountsOneSecond;
			}

			// Update the SubSeconds field to one second in us,
			// by scaling the delta of the counter and counts to tick.
			timestamp.SubSeconds = (((uint64_t)(timestamp.SubSeconds - TickCounter)) * ONE_SECOND_MICROS) / CountsOneSecond;
		}
		else
		{
			// On tickless sources (i.e. micros()), compensate with external overflow count.
			const uint32_t overflows = ClockSource->GetTicklessOverflows();
			timestamp.Seconds += overflows * MICROS_OVERFLOW_WRAP_SECONDS;
			timestamp.SubSeconds += overflows * MICROS_OVERFLOW_WRAP_REMAINDER;
		}

		timestamp.ConsolidateOverflow();

		// Add local and parameter offset.
		timestamp.ShiftSubSeconds(OffsetMicros);
	}

	void ShiftMicros(const int32_t offsetMicros)
	{
		OffsetMicros += offsetMicros;
	}
};
#endif