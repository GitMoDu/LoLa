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
		SetMillis(random(UINT32_MAX));
	}

	void SetMillis(const uint32_t timeMillis)
	{
		LastSetMillis = millis();
		Offset = timeMillis - LastSetMillis;
	}

	void AddOffset(const int16_t offset)
	{
		Offset += offset;
	}

	uint32_t GetMillis()
	{
		return millis() + Offset;
	}

	uint32_t GetMicros()
	{
		return GetMillis()*1000L + micros();
	}

	uint32_t GetLastSyncElapsedMillis()
	{
		if (LastSetMillis != 0)
		{
			return millis() - LastSetMillis;
		}
		return UINT32_MAX;
	}
};

#endif