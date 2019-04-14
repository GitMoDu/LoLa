// ILoLaClockSource.h

#ifndef _ILOLA_CLOCKSOURCE_h
#define _ILOLA_CLOCKSOURCE_h

#include <stdint.h>
#include <Arduino.h>



class ILoLaClockSource
{
public:
	static const uint32_t ONE_SECOND_MICROS = 1000000;
	static const uint32_t ONE_SECOND = 1;

private:
	uint32_t OffsetSeconds = 0;
	int32_t OffsetMicros = 0;
	uint32_t DeferredOffsetMicros = 0;

	int32_t DriftCompensationMicros = 0;

	static const int32_t MAX_OFFSET_MICROS = ONE_SECOND_MICROS;
	static const int32_t MAX_DRIFT_MICROS = 200000;

	uint32_t LastTick = 0;

	bool Attached = false;

	//Helper.
	uint32_t OffsetChunk = 0;

protected:
	virtual uint32_t GetMicros()
	{
		return micros();
	}

	virtual void Attach() {	}
	virtual void OnDriftUpdated(const int32_t driftMicros) {}

	virtual uint32_t GetUTCSeconds() { return millis() / 1000; }
	virtual void SetUTCSeconds(const uint32_t secondsUTC) {}

public:
	ILoLaClockSource()
	{
		Reset();
		Attached = false;
	}

	void Tick()
	{
		LastTick = GetMicros();
		AddOffsetMicros(ONE_SECOND_MICROS + DriftCompensationMicros - DeferredOffsetMicros);
		DeferredOffsetMicros = 0;
	}

	void Start()
	{
		if (!Attached)
		{
			Attached = true;
			Attach();
		}
	}

	void Reset()
	{
		OffsetSeconds = 0;
		OffsetMicros = 0;
		DriftCompensationMicros = 0;
		DeferredOffsetMicros = 0;
		LastTick = GetMicros();
	}

	void SetUTC(const uint32_t secondsUTC)
	{
		SetUTCSeconds(secondsUTC);
		LastTick = GetMicros();
		OffsetMicros = 0;
		DeferredOffsetMicros = 0;

		//TODO: Readjust OffsetSeconds to keep sync seconds aligned.
	}

	uint32_t GetUTC()
	{
		return GetUTCSeconds();
	}

	uint32_t GetSyncSeconds()
	{
		asm volatile("nop"); //allow interrupt to fire
		asm volatile("nop");

		return OffsetSeconds +
			((GetElapsedMicros() + OffsetMicros) / ONE_SECOND_MICROS);
	}

	void SetSyncSeconds(const uint32_t syncSeconds)
	{
		OffsetSeconds = syncSeconds;
		OffsetMicros = 0;
		DeferredOffsetMicros = 0;
		LastTick = GetMicros();
	}

	void SetRandom()
	{
		OffsetSeconds = random(INT32_MAX);
		OffsetMicros = random(INT32_MAX);
		LastTick = GetMicros();
	}

	void AddOffsetMicros(const int32_t offset)
	{
		if (offset >= 0)
		{
			OffsetMicros += offset - OffsetChunk;

			while (OffsetMicros > ONE_SECOND_MICROS)
			{
				OffsetSeconds++;
				LastTick = GetMicros();
				OffsetMicros -= ONE_SECOND_MICROS;
			}
		}
		else
		{
			DeferredOffsetMicros = min(DeferredOffsetMicros - offset, ONE_SECOND_MICROS);
		}
	}

	void SetDriftCompensationMicros(const int32_t driftMicros)
	{
		DriftCompensationMicros = constrain(driftMicros, -MAX_DRIFT_MICROS, MAX_DRIFT_MICROS);
		OnDriftUpdated(DriftCompensationMicros);
	}

	int32_t GetDriftCompensationMicros()
	{
		return DriftCompensationMicros;
	}

	void AddDriftCompensationMicros(const int32_t driftMicros)
	{
		DriftCompensationMicros = constrain(DriftCompensationMicros + driftMicros, -MAX_DRIFT_MICROS, MAX_DRIFT_MICROS);
		OnDriftUpdated(DriftCompensationMicros);
	}

	uint32_t GetSyncMicrosFull()
	{
		asm volatile("nop"); //allow interrupt to fire
		asm volatile("nop");

		return (OffsetSeconds*ONE_SECOND_MICROS) + GetElapsedMicros() + OffsetMicros - DeferredOffsetMicros;
	}

	uint32_t GetSyncMicros()
	{
		asm volatile("nop"); //allow interrupt to fire
		asm volatile("nop");

		return (OffsetSeconds*ONE_SECOND_MICROS) + GetElapsedMicros() + OffsetMicros;
	}

private:
	inline uint32_t GetElapsedMicros()
	{
		return GetMicros() - LastTick;
	}
};

#endif