// LoLaSyncedClock.h

#ifndef _LOLA_SYNCED_CLOCK_h
#define _LOLA_SYNCED_CLOCK_h

#include <LoLaClock\IClock.h>

class LoLaSyncedClock : public ISyncedClock
{
private:
	volatile uint32_t SecondsCounter = 0;
	volatile uint32_t OffsetMicros = 0;
	volatile uint32_t LastTimeStamp = 0;

protected:
	IClockTickSource* TickSource = nullptr;
	IClockMicrosSource* TimestampSource = nullptr;

public:
	LoLaSyncedClock() : ISyncedClock()
	{
	}

	bool Setup()
	{
		if (TickSource != nullptr &&
			TimestampSource != nullptr)
		{
			TickSource->SetupTickInterrupt();

			return true;
		}
	}

	void OnTick()
	{
		LastTimeStamp = TimestampSource->GetMicros();
		SecondsCounter++;
	}

	ISyncedCallbackSource* GetSyncedCallbackSource()
	{
		return TimestampSource;
	}

protected:
	// Reserved for forced timestamping during training, for increased accuracy.
	void OnTick(uint32_t timestamp)
	{
		LastTimeStamp = timestamp;
		SecondsCounter++;
	}

public:
	virtual uint32_t GetSyncSeconds()
	{
		// Allow Tick interrupt to fire, if pending.
		asm volatile("nop");
		asm volatile("nop");

		return SecondsCounter;
	}

	virtual uint64_t GetSyncMillis()
	{
		// Allow Tick interrupt to fire, if pending.
		asm volatile("nop");
		asm volatile("nop");

		return (TimestampSource->GetMicros()
			- LastTimeStamp
			+ (SecondsCounter * ONE_SECOND_MICROS)
			+ OffsetMicros)
			/ ONE_MILLI_MICROS;
	}

	virtual uint64_t GetSyncMicros()
	{
		// Allow Tick interrupt to fire, if pending.
		asm volatile("nop");
		asm volatile("nop");

		// We read the subseconds first for best accuracy.
		// Next we add the global, slow seconds adjusted for time scale.
		// Finally we add the sub-second offset.
		return TimestampSource->GetMicros()
			- LastTimeStamp
			+ (SecondsCounter * ONE_SECOND_MICROS)
			+ OffsetMicros;
	}

	virtual void SetTuneOffset(const int32_t tuneOffset)
	{
		TickSource->SetTuneOffset(tuneOffset);
		TimestampSource->OnTuneUpdated();
	}

	virtual void SetOffsetSeconds(const uint32_t offsetSeconds)
	{
		SecondsCounter = offsetSeconds;
	}

	virtual void AddOffsetSeconds(const int32_t offsetSeconds)
	{
		SecondsCounter += offsetSeconds;
	}

	virtual void SetOffsetMicros(const int32_t offsetMicros)
	{
		SecondsCounter += offsetMicros / ONE_SECOND_MICROS;
		OffsetMicros += offsetMicros % ONE_SECOND_MICROS;

		if (OffsetMicros > ONE_SECOND_MICROS)
		{
			SecondsCounter += OffsetMicros / ONE_SECOND_MICROS;
			OffsetMicros = OffsetMicros % ONE_SECOND_MICROS;
		}
	}

	void AddOffsetMicros(const int32_t offsetMicros)
	{
		AddOffsetSeconds(offsetMicros / ONE_SECOND_MICROS);
		OffsetMicros = offsetMicros % ONE_SECOND_MICROS;
	}
};
#endif