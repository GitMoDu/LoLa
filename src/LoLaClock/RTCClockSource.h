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
	uint32_t LastTime = 0;

	const rtc_clk_src ClockSourceType = RTCSEL_LSI;

private:
	void Attach();

	bool Attached = false;

private:
	inline uint32_t GetBaseRTC()
	{
		return (LastTime * ((uint32_t)1000000));
	}

protected:
	//uint32_t GetCurrentsMicros() { return GetBaseRTC() + (micros() - LastTick); }

public:
	RTCClockSource();

	void Start()
	{
		if (!Attached)
		{
			Attached = true;
			Attach();
		}
		LastTick = 0;
		LastTime = 0;
	}

	void OnInterrupt()
	{
		LastTick = micros();
		LastTime++;
	}
};
#endif