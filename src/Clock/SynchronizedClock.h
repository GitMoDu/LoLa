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


	/// <summary>
	/// Offsets that determine the current clock result.
	/// </summary>
private:
	int32_t OffsetMicros = 0;

	uint32_t LastTickCounter = 0;
	volatile uint32_t TickCounter = 0;
	volatile uint32_t CountsOneSecond = 0;
	volatile uint32_t Seconds = 0;

	/// <summary>
	/// Offsets to keep Seconds monotonic in-line with the micros source.
	/// </summary>
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
		, ClockSource(clockSource)
		, TimerSource(timerSource)
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

	const uint32_t GetSeconds(const int32_t offsetMicros)
	{
		static Timestamp timestamp;

		GetTimestamp(timestamp);
		timestamp.ShiftSubSeconds(offsetMicros);

		return timestamp.Seconds;
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
			// Update the SubSeconds field by
			// scaling the delta of the counter and counts to tick,
			// to one second in us.

			if (timestamp.SubSeconds < TickCounter)
			{
				//Overflow detected and compensated before final subseconds calculation.
				timestamp.Seconds++;
				timestamp.SubSeconds += CountsOneSecond;
			}

			timestamp.SubSeconds = (((uint64_t)(timestamp.SubSeconds - TickCounter)) * ONE_SECOND_MICROS) / CountsOneSecond;
		}
		else
		{
			// On tickless sources (i.e. micros()), compensate with external overflow count.
			uint32_t overflows = ClockSource->GetTicklessOverflows();
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

	void CalculateError(TimestampError& error, const Timestamp& estimation, const int32_t offsetMicros)
	{
		static Timestamp timestamp;
		GetTimestamp(timestamp);
		timestamp.ShiftSubSeconds(offsetMicros);


		error.Seconds = timestamp.Seconds - estimation.Seconds;
		error.SubSeconds = timestamp.SubSeconds - estimation.SubSeconds;

		// Consolidate SubSeconds.
		while (error.SubSeconds < 0)
		{
			error.SubSeconds += ONE_SECOND_MICROS;
			error.Seconds--;
		}
		while (error.SubSeconds > ONE_SECOND_MICROS)
		{
			error.SubSeconds -= ONE_SECOND_MICROS;
			error.Seconds++;
		}

#if defined(DEBUG_LOLA)
		//Serial.print(F("\tComparing estimation:"));
		//Serial.print(estimation.Seconds);
		//Serial.print('s');
		//Serial.print(estimation.SubSeconds);
		//Serial.println(F("us"));
		//Serial.print(F("\tto "));
		//Serial.print(timestamp.Seconds);
		//Serial.print('s');
		//Serial.print(timestamp.SubSeconds);
		//Serial.println(F("us"));

		//Serial.print(F("\tResult: "));
		//Serial.print(error.Seconds);
		//Serial.print('s');
		//if (error.SubSeconds >= 0) {
		//	Serial.print('+');
		//}
		//Serial.print(error.SubSeconds);
		//Serial.println(F("us"));
#endif
	}
};
#endif