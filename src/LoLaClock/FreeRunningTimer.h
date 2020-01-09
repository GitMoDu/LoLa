// FreeRunningTimer.h

#ifndef _IFREERUNNINGTIMER_h
#define _IFREERUNNINGTIMER_h


#if defined(ARDUINO_ARCH_STM32F1)
#include <LoLaClock\FreeRunningTimers\STM32FreeRunningTimer.h>
#elif defined(ARDUINO_ARCH_AVR)
#include <LoLaClock\FreeRunningTimers\ArduinoMicrosFreeRunningTimer.h>
#else
#error No precision timer for this platform.
#endif

#endif