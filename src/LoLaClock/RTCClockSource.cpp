//RTCClockSource.cpp

#include <LoLaClock\RTCClockSource.h>

RTCClockSource* StaticRTC = nullptr;

static void StaticRTCInterrupt()
{
	StaticRTC->Tick();
}

////////////////
RTCClockSource::RTCClockSource()
	: RTC(RTCSEL_LSI), LoLaTimerClockSource()
{
	StaticRTC = this;
}

void RTCClockSource::Attach()
{
	LoLaTimerClockSource::Attach();
	RTC.attachSecondsInterrupt(StaticRTCInterrupt);
}

uint32_t RTCClockSource::GetUTCSeconds() 
{ 
	return RTC.getTime();
}

void RTCClockSource::SetUTCSeconds(const uint32_t secondsUTC)
{
	RTC.setTime((time_t)secondsUTC);
}


