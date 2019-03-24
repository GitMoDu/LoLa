//RTCClockSource.cpp

#include <LoLaClock\RTCClockSource.h>



RTCClockSource* StaticRTC = nullptr;

static void OnRTCInterrupt()
{
	StaticRTC->OnInterrupt();
}

////////////////
RTCClockSource::RTCClockSource() 
	: RTC(RTCSEL_LSI), ILoLaClockSource()
{
	StaticRTC = this;
}

void RTCClockSource::Attach()
{
	RTC.attachSecondsInterrupt(OnRTCInterrupt);
}