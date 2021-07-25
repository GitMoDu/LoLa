// STM32RTCClockTicker.cpp
//

#if defined(ARDUINO_ARCH_STM32F1)
#include <LoLaClock\ClockTickers\STM32RTCClockTicker.h>

TickTrainedTimerSyncedClock* StaticClockTicker = nullptr;

void StaticOnRTCTick()
{
	StaticClockTicker->OnTick();
}

void ClockTicker::SetupTickInterrupt()
{
	StaticClockTicker = this;
	RTCInstance.attachSecondsInterrupt(StaticOnRTCTick);
}
#endif