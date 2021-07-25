// ClockTickSource.h

#ifndef _CLOCK_TICK_SOURCE_h
#define _CLOCK_TICK_SOURCE_h

#include <LoLaDefinitions.h>


#if defined(LOLA_CLOCK_TICK_FROM_GPS) && !defined(LOLA_CLOCK_TICK_FROM_RTC)
	#error This precision timer is not available.
	//#include <LoLaClock\ClockTickers\GPSClockTicker.h> // TODO:
#elif defined(LOLA_CLOCK_TICK_FROM_GPS) && defined(LOLA_CLOCK_TICK_FROM_RTC)
	#error This precision timer is not available.
	//#include <LoLaClock\ClockTickers\GPSWithRTCFallbackClockTicker.h> // TODO:
#elif !defined(LOLA_CLOCK_TICK_FROM_GPS) && defined(LOLA_CLOCK_TICK_FROM_RTC)
	#if defined(ARDUINO_ARCH_STM32F1)
			#include <LoLaClock\ClockTickers\STM32RTCClockTicker.h>
	#else
			#include <LoLaClock\ClockTickers\TimerOneClockTicker.h>
	#endif
#else
	#error No precision timer available.
#endif

#endif