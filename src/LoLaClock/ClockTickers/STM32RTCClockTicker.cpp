//RTCClockSource.cpp
//
#include <LoLaClock\ClockTickers\STM32RTCClockTicker.h>
#include <Arduino.h>

static ISyncedClock* StaticClockTickerTarget = nullptr;

static void StaticOnRTCTick()
{
	StaticClockTickerTarget->OnTick();
}

static void StaticOnRTCTrainingTick()
{
	StaticClockTickerTarget->OnTrainingTick();
}

ClockTicker::ClockTicker() : IClockTickSource()
	, RTCInstance(ClockSourceType)
{
}

bool ClockTicker::Setup(ISyncedClock* target)
{
	StaticClockTickerTarget = target;

	return StaticClockTickerTarget != nullptr;
}

void ClockTicker::SetupTrainingTickInterrupt()
{
	RTCInstance.attachSecondsInterrupt(StaticOnRTCTrainingTick);
}

void ClockTicker::SetupTickInterrupt()
{
	RTCInstance.attachSecondsInterrupt(StaticOnRTCTick);
}