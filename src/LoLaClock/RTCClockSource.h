// RTCClockSource.h

#ifndef _RTC_CLOCKSOURCE_h
#define _RTC_CLOCKSOURCE_h

#include <LoLaClock\ILolaClockSource.h>
#include <RTClock.h>


class RTCClockSource : public ILoLaClockSource
{
private:
	RTClock RTC;

	uint32_t LastTick = 0;
	uint32_t LastSeconds = 0;

	const rtc_clk_src ClockSourceType = RTCSEL_LSI;

private:
	void Attach();

	bool Attached = false;

private:
	inline uint32_t GetBaseRTC()
	{
		//Use last 16 bits of seconds as rolling seconds source.
		return ((LastSeconds & 0xFFFF) * ((uint32_t)1000000));
	}

protected:
	uint32_t GetCurrentsMicros() { return GetBaseRTC() + (micros() - LastTick); }

public:
	RTCClockSource();

	uint32_t GetTimeSeconds()
	{
		return LastSeconds;
	}

	void Start()
	{
		if (!Attached)
		{
			Attached = true;
			Attach();
		}
		LastTick = 0;
		LastSeconds = 0;
	}

	void OnInterrupt()
	{
		LastTick = micros();
		LastSeconds++;
	}
};
#endif