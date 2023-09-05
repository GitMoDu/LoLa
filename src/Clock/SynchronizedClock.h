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
	uint32_t OffsetMicros = 1;

	uint32_t LastTickCounter = 0;
	volatile uint32_t TickCounter = 0;
	volatile uint32_t CountsOneSecond = 0;
	volatile uint32_t Seconds = 0;

	int16_t TuneMicros = 0;

private:
	/// <summary>
	/// 32 bit, us clock source.
	/// </summary>
private:
	ITimerSource* TimerSource;
	IClockSource* ClockSource;


public:
	SynchronizedClock(IClockSource* clockSource, ITimerSource* timerSource)
		: IClockSource::IClockListener()
		, TimerSource(timerSource)
		, ClockSource(clockSource)
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
		Seconds += offsetSeconds;
	}

	void GetTimestampMonotonic(Timestamp& timestamp)
	{
		// Get the latest clock seconds and timer counter.
		timestamp.SubSeconds = TimerSource->GetCounter();
		const uint32_t overflows = ClockSource->GetTicklessOverflows();

		timestamp.Seconds = 0;
		timestamp.ConsolidateSubSeconds();

		timestamp.Seconds += Seconds;

		// Compensate with external overflow count.
		timestamp.Seconds += overflows * MICROS_OVERFLOW_WRAP_SECONDS;
		timestamp.SubSeconds += overflows * MICROS_OVERFLOW_WRAP_REMAINDER;
		timestamp.ConsolidateSubSeconds();
	}

	void GetTimestamp(Timestamp& timestamp)
	{
		// Get the latest monotonic timestamp.
		GetTimestampMonotonic(timestamp);

		// Add local and parameter offset.
		timestamp.ShiftSubSeconds(OffsetMicros);
	}

	void ShiftTuneMicros(const int16_t offsetMicros)
	{
		TuneMicros += offsetMicros;
		if (TuneMicros > LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
		{
			TuneMicros = LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS;
		}
		else if (TuneMicros < -LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
		{
			TuneMicros = -LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS;
		}

		ClockSource->TuneClock(TuneMicros);
	}

	void ShiftMicros(const int32_t offsetMicros)
	{
		OffsetMicros += offsetMicros;

		if (OffsetMicros >= ONE_SECOND_MICROS)
		{
			Seconds += OffsetMicros / ONE_SECOND_MICROS;
			OffsetMicros %= ONE_SECOND_MICROS;
		}
	}
};
#endif