//RTCClockSource.cpp
//
#include <LoLaClock\ClockTickers\STM32RTCClockTicker.h>

static IClockTickTarget* StaticTarget = nullptr;

void StaticOnRTCTick()
{
	StaticTarget->OnTick();
}

void StaticOnRTCTrainingTick()
{
	StaticTarget->OnTrainingTick();
}

ClockTicker::ClockTicker() : IClockTickSource()
	, RTC(ClockSourceType)
{
}

bool ClockTicker::Setup(IClockTickTarget* target)
{
	StaticTarget = target;

	return StaticTarget != nullptr;
}

void ClockTicker::SetupTrainingTickInterrupt()
{
	RTC.attachSecondsInterrupt(StaticOnRTCTrainingTick);
}

void ClockTicker::SetupTickInterrupt()
{
	RTC.attachSecondsInterrupt(StaticOnRTCTick);
}