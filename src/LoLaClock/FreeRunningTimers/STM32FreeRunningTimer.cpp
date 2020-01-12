//STM32FreeRunningTimer.cpp
//
#include <LoLaClock\FreeRunningTimers\STM32FreeRunningTimer.h>

FreeRunningTimer* StaticTarget = nullptr;

static void StaticOnTimerCompare()
{
	StaticTarget->OnCompareInterrupt();
}

FreeRunningTimer::FreeRunningTimer()
	: IFreeRunningTimer()
	, Timer(TimerIndex)
{
	StaticTarget = this;
}

void FreeRunningTimer::StartCallbackAfterSteps(const uint32_t steps)
{
	Timer.setCompare(TimerIndex, (Timer.getCount() + steps) % UINT16_MAX);
	Timer.attachCompare1Interrupt(StaticOnTimerCompare);
}

