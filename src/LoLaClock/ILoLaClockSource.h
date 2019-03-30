// ILoLaClockSource.h

#ifndef _ILOLA_CLOCKSOURCE_h
#define _ILOLA_CLOCKSOURCE_h

#include <stdint.h>
#include <Arduino.h>

class ILoLaClockSource
{
private:
	uint32_t OffsetMicros = 0;

protected:
	virtual uint32_t GetCurrentsMicros() { return micros(); }

public:
	virtual void Start() {};
	virtual uint32_t GetTimeSeconds() { return millis() / 1000; }

public:
	ILoLaClockSource()
	{
		Reset();
	}

	void Reset()
	{
		OffsetMicros = 0;
	}

	void SetRandom()
	{
		OffsetMicros = (uint32_t)random(INT32_MAX);
	}

	void AddOffsetMicros(const int32_t offset)
	{
		OffsetMicros = OffsetMicros + offset;
	}

	uint32_t GetSyncMicros()
	{
		return GetCurrentsMicros() + OffsetMicros;
	}

	uint32_t GetSyncMicros(const uint32_t sourceMicros)
	{
		return sourceMicros + OffsetMicros;
	}
};

#endif