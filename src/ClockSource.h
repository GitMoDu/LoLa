// ClockSource.h

#ifndef _CLOCKSOURCE_h
#define _CLOCKSOURCE_h

#include <stdint.h>

class ClockSource
{
private:
	uint32_t OffsetMicros = 0;

public:
	ClockSource()
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
		return micros() + OffsetMicros;
	}

	uint32_t GetSyncMicros(const uint32_t sourceMicros)
	{
		return sourceMicros + OffsetMicros;
	}
};

#endif