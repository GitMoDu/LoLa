// RTCClockSource.h

#ifndef _RTC_CLOCKSOURCE_h
#define _RTC_CLOCKSOURCE_h

#include <LoLaClock\LoLaTimerClockSource.h>
#include <RTClock.h>

class RTCClockSource : public LoLaTimerClockSource
{
private:
	RTClock RTC;
	const rtc_clk_src ClockSourceType = RTCSEL_LSI;

protected:
	void Attach();

	uint32_t GetUTCSeconds();
	void SetUTCSeconds(const uint32_t secondsUTC);

public:
	RTCClockSource();

};
#endif