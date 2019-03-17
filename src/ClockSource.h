// ClockSource.h

#ifndef _CLOCKSOURCE_h
#define _CLOCKSOURCE_h

#include <stdint.h>

class ClockSource
{
private:
	uint32_t Offset = 0;
	uint32_t LastSetMillis = 0;

public:
	ClockSource()
	{
		Reset();
	}

	void Reset()
	{
		Offset = 0;
		LastSetMillis = 0;
	}

	void SetRandom()
	{
		Offset = (uint32_t)random(INT32_MAX);
	}

	void SetMillis(const uint32_t timeMillis)
	{
		LastSetMillis = millis();
		Offset = timeMillis - LastSetMillis;
	}

	void AddOffset(const int32_t offset)
	{
		Offset = Offset + offset;
	}

	uint32_t GetSyncMillis(const uint32_t sourceMillis)
	{
		return sourceMillis + Offset;
	}

	uint32_t GetSyncMillis()
	{
		return millis() + Offset;
	}

};

#endif