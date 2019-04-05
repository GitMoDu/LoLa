// ILoLaClockSource.h

#ifndef _ILOLA_CLOCKSOURCE_h
#define _ILOLA_CLOCKSOURCE_h

#include <stdint.h>
#include <Arduino.h>

#define LOLA_CLOCK_SOURCE_MILLIS

class ILoLaClockSource
{
private:
	uint32_t Seconds = 0;
	uint32_t LastTick = 0;

	int32_t OffsetMicros = 0;

	int32_t DriftCompensationMicros = 0;

	static const uint32_t OneSecondMicros = 1000000;
	static const int32_t MaxOffsetMicros = OneSecondMicros;
	static const int32_t MaxDriftMicros = 100000;
	static const uint32_t OneSecond = 1;

	uint32_t TickHelper = 0;


private:
	inline uint32_t GetElapsedMicros()
	{
		TickHelper = micros();

		return TickHelper - min(TickHelper, LastTick);
	}

protected:
	void Tick()
	{
		LastTick = micros();
		Seconds++;
		AddOffsetMicros(DriftCompensationMicros);
	}

public:
	virtual void Start() {}
	virtual uint32_t GetUTCSeconds() { return Seconds; }

public:
	ILoLaClockSource()
	{
		Reset();
	}

	uint32_t GetTimeSeconds()
	{
		return Seconds + ((GetElapsedMicros() + OffsetMicros) / OneSecondMicros);
	}

	void Reset()
	{
		Seconds = 0;
		OffsetMicros = 0;
		LastTick = micros();
	}

	void SetTimeSeconds(const uint32_t timeSeconds) 
	{
		Seconds = timeSeconds;
		OffsetMicros = 0;
		LastTick = micros();
	}

	void SetRandom()
	{
		Seconds = random(INT32_MAX);
		OffsetMicros = 0;
		LastTick = micros();
	}

	void ClearOffsetMicros()
	{
		OffsetMicros = 0;
	}

	int32_t GetOffsetMicros()
	{
		return OffsetMicros;
	}

	void AddOffsetMicros(const int32_t offset)
	{
		OffsetMicros += offset;

		while (abs(OffsetMicros) >= MaxOffsetMicros)
		{
			if (OffsetMicros >= MaxOffsetMicros)
			{
				OffsetMicros -= OneSecondMicros;
				Seconds += OneSecond;
			}
			else if (abs(OffsetMicros) <= MaxOffsetMicros)
			{
				OffsetMicros += OneSecondMicros;
				Seconds -= OneSecond;
			}
		}
	}

	void AddDriftCompensationMicros(const int32_t driftMicros)
	{
		DriftCompensationMicros += constrain(driftMicros, -MaxDriftMicros, MaxDriftMicros);

		DriftCompensationMicros = constrain(DriftCompensationMicros, -MaxDriftMicros, MaxDriftMicros);
		Serial.print(F("Adjusted drift: "));
		Serial.println(DriftCompensationMicros);
	}

	uint32_t GetSyncMicros()
	{
		return GetPrunedSecondsInMicros() + GetElapsedMicros() + OffsetMicros;
	}

private:
	uint32_t GetPrunedSecondsInMicros()
	{
		return (Seconds) * OneSecondMicros;
	}
};

#endif