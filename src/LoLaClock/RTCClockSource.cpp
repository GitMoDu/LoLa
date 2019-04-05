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

void RTCClockSource::Start()
{
	if (!Attached)
	{
		Attached = true;
		Attach();
		SetTimeSeconds(RTC.getTime());
	}
}

uint32_t RTCClockSource::GetUTCSeconds() 
{ 
	return RTC.getTime();
}

void RTCClockSource::OnInterrupt()
{
	Tick();
}
