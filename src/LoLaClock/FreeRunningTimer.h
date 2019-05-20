// FreeRunningTimer.h

#ifndef _FREERUNNINGTIMER_h
#define _FREERUNNINGTIMER_h


#ifdef ARDUINO_ARCH_STM32F1

#include <libmaple/rcc.h>
#include "ext_interrupts.h" // for noInterrupts(), interrupts()
#include "wirish_math.h"
#include <board/board.h>           // for CYCLES_PER_MICROSECOND



#define MAX_RELOAD ((1 << 16) - 1)
uint32 SetMinimumUnitTarget(const uint32 microseconds = 5) {
	// Not the best way to handle this edge case?
	if (microseconds< 1) {
		return false;
	}

	uint32 period_cyc = microseconds * CYCLES_PER_MICROSECOND;
	uint16 prescaler = (uint16)(period_cyc / MAX_RELOAD + 1);
	uint16 overflow = (uint16)((period_cyc + (prescaler / 2)) / prescaler);
	//this->setPrescaleFactor(prescaler);
	//this->setOverflow(overflow);g

	//return true;
	return overflow;
}

#endif

#endif

