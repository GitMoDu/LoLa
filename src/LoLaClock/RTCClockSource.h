// RTCClockSource.h

#ifndef _RTC_CLOCKSOURCE_h
#define _RTC_CLOCKSOURCE_h

#include <LoLaClock\ILolaClockSource.h>

class RTCClockSource : public ILoLaClockSource
{
private:

protected:
	virtual uint32_t GetCurrentsMicros() { return micros(); }

public:
	RTCClockSource() : ILoLaClockSource()
	{
	}
};

#endif