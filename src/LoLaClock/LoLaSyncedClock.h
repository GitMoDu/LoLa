// LoLaSyncedClock.h

#ifndef _LOLA_SYNCED_CLOCK_h
#define _LOLA_SYNCED_CLOCK_h

#include <LoLaClock\IClock.h>
#include <LoLaClock\FreeRunningTimer.h>
#include <LoLaClock\ClockTicker.h>

class LoLaSyncedClock : public ISyncedClock
{
private:
	const uint32_t MaxTimerRange = INT32_MAX;

	// Helpers for getters.
	uint32_t SyncStepHelper = 0;
	uint32_t SyncSecondsHelper = 0;
	uint64_t SyncHelper = 0;

protected:
	// Clock and Timer.
	ClockTicker TickerSource;
	FreeRunningTimer TimerSource;
	uint32_t TimerRange = MaxTimerRange;

protected:
	// Live time data.
	volatile uint32_t SecondsCounter = 0;
	volatile uint32_t SecondsStep = 0;
	volatile uint32_t StepsPerSecond = MaxTimerRange;
	uint32_t OffsetMicros = 0;

protected:
	virtual void OnTuneUpdated() {}

public:
	LoLaSyncedClock() : ISyncedClock()
	{
	}

	bool Setup()
	{
		if (TimerSource.SetupTimer() &&
			TickerSource.Setup(this) &&
			TimerSource.GetTimerRange() <= MaxTimerRange)
		{
			TimerRange = TimerSource.GetTimerRange();
#ifdef DEBUG_LOLA_CLOCK
			pinMode(DEBUG_LOLA_CLOCK_DEBUG_PIN, OUTPUT);
			digitalWrite(DEBUG_LOLA_CLOCK_DEBUG_PIN, LOW);
#endif

			return true;
		}

		return false;
	}

	virtual void OnTick()
	{
		// Increment the counter then divide by UINT32_MAX and return only the remainder.
		SecondsCounter = (SecondsCounter + 1) % UINT32_MAX;
		SecondsStep = TimerSource.GetStep();
#ifdef DEBUG_LOLA_CLOCK
		digitalWrite(DEBUG_LOLA_CLOCK_DEBUG_PIN, !digitalRead(DEBUG_LOLA_CLOCK_DEBUG_PIN));
#endif
	}

	virtual uint32_t GetSyncSeconds()
	{
		return SecondsCounter;
	}

	virtual uint64_t GetSyncMillis()
	{
		// Capture pair of values, guaranteed cohesion.
		// Minimal interrupt disruption, only 2 copies.
		noInterrupts();
		SyncSecondsHelper = SecondsCounter;
		SyncStepHelper = TimerSource.GetStep();
		interrupts();

		if (SyncStepHelper < SecondsStep)
		{
			SyncHelper = (TimerRange - SecondsStep) + SyncStepHelper;
		}
		else
		{
			SyncHelper = SyncStepHelper - SecondsStep;
		}

		SyncHelper = ((SyncHelper + OffsetMicros) * ONE_SECOND_MILLIS) / StepsPerSecond;
		SyncHelper += SyncSecondsHelper * ONE_SECOND_MILLIS;

		return SyncHelper;
	}

	virtual uint64_t GetSyncMicros()
	{
		// Capture pair of values, guaranteed cohesion.
		// Minimal interrupt disruption, only 2 copies.
		noInterrupts();
		SyncSecondsHelper = SecondsCounter;
		SyncStepHelper = TimerSource.GetStep();
		interrupts();

		if (SyncStepHelper < SecondsStep)
		{
			SyncHelper = (TimerRange - SecondsStep) + 1 + SyncStepHelper;
		}
		else
		{
			SyncHelper = SyncStepHelper - SecondsStep;
		}

		SyncHelper = (SyncHelper * ONE_SECOND_MICROS) / StepsPerSecond;
		SyncHelper += SyncSecondsHelper * ONE_SECOND_MICROS;

		SyncHelper += OffsetMicros;

		return SyncHelper;
	}

	virtual void SetTuneOffset(const int32_t offsetMicros)
	{
		TickerSource.SetTuneOffset(offsetMicros);
		OnTuneUpdated();
	}

	virtual void SetOffsetSeconds(const uint32_t offsetSeconds)
	{
		SecondsCounter = offsetSeconds;
	}

	virtual void AddOffsetMicros(const int32_t offsetMicros)
	{
		AddOffsetSeconds(offsetMicros / ONE_SECOND_MICROS);
		int32_t SubSecondOffset = (offsetMicros % ONE_SECOND_MICROS);
		if (offsetMicros >= 0)
		{
			OffsetMicros += SubSecondOffset;
			PruneOffsetMicros();
		}
		else
		{
			if (OffsetMicros + SubSecondOffset < 0)
			{
				AddOffsetSeconds(-1);
				OffsetMicros = ONE_SECOND_MICROS - SubSecondOffset;
			}
			else
			{
				OffsetMicros += SubSecondOffset;
				PruneOffsetMicros();
			}
		}
	}

private:
	void PruneOffsetMicros()
	{
		if (OffsetMicros >= ONE_SECOND_MICROS)
		{
			AddOffsetSeconds(1);
			OffsetMicros = OffsetMicros % ONE_SECOND_MICROS;
		}
	}

	virtual void AddOffsetSeconds(const int32_t offsetSeconds)
	{
		if (offsetSeconds >= 0)
		{
			// Increment the counter then divide by UINT32_MAX and return only the remainder.
			SecondsCounter = (SecondsCounter + offsetSeconds) % UINT32_MAX;
		}
		else
		{
			// Down-counter decrements the counter to zero then rolls over to UINT32_MAX.
			SecondsCounter = (SecondsCounter + (UINT32_MAX + offsetSeconds)) % UINT32_MAX;
		}
	}
};
#endif