//STM32FreeRunningTimer.cpp
//

#if defined(ARDUINO_ARCH_STM32F1)
#include <LoLaClock\FreeRunningTimers\STM32FreeRunningTimer.h>

FreeRunningTimer* StaticFreeRunningTimer = nullptr;

static void StaticOnFreerunningTimerCompare()
{
	StaticFreeRunningTimer->OnCompareInterrupt();
}

bool FreeRunningTimer::SetupInterrupts()
{
	StaticFreeRunningTimer = this;
}

void FreeRunningTimer::StartCallbackAfterSteps(const uint16_t steps)
{
	if (CallbackTarget != nullptr)
	{
		Detach();
		DeviceTimer.setCompare(TimerChannelIndex, (DeviceTimer.getCount() + steps) % UINT16_MAX);
		DeviceTimer.attachCompare1Interrupt(StaticOnFreerunningTimerCompare);
	}
}
#endif
