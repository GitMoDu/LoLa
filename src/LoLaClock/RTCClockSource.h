// RTCClockSource.h

#ifndef _RTC_CLOCKSOURCE_h
#define _RTC_CLOCKSOURCE_h

#include <LoLaClock\ILolaClockSource.h>
#include <RTClock.h>


class RTCClockSource : public ILoLaClockSource
{
private:
	RTClock RTC;

	bool Attached = false;

	const rtc_clk_src ClockSourceType = RTCSEL_LSI;

private:
	void Attach();

public:
	RTCClockSource();

	void Start();

	uint32_t GetUTCSeconds();

	void OnInterrupt();
};
#endif