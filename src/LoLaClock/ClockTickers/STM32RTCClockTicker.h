// STM32RTCClockTicker.h

#if !defined(_STM32_RTC_CLOCK_TICKER_h) && defined(ARDUINO_ARCH_STM32F1)
#define _STM32_RTC_CLOCK_TICKER_h

#include <LoLaClock\TickTrainedTimerSyncedClock.h>
#include <RTClock.h>

class ClockTicker : public TickTrainedTimerSyncedClock
{
private:
	RTClock RTCInstance;
	static const rtc_clk_src ClockSourceType = RTCSEL_LSI;

private:
	// Defined in Cpp.
	void SetupTickInterrupt();

public:
	ClockTicker()
		: RTCInstance(ClockSourceType)
		, TickTrainedTimerSyncedClock()
	{
	}

	virtual bool Setup()
	{
		if (TickTrainedTimerSyncedClock::Setup())
		{
			// Setup a interrupt callback to OnTick(), to train the timer on the accurate 1 PPS signal.
			SetupTickInterrupt();

			return true;
		}
		else
		{
			return false;
		}
	}

	virtual void SetTuneOffset(const int32_t offsetMicros)
	{
		// TODO: convert units?
		SetTickerTuneOffset(offsetMicros);
	}



protected:
	void DetachAll()
	{
		RTCInstance.detachAlarmInterrupt();
		RTCInstance.detachSecondsInterrupt();
	}

	virtual void SetTickerTuneOffset(const int32_t offsetMicros)
	{
		//TODO: https://github.com/ag88/stm32duino_rtcadj
	}
};
#endif