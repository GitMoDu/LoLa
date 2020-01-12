// STM32RTCClockTicker.h

#ifndef _STM32_RTC_CLOCK_TICKER_h
#define _STM32_RTC_CLOCK_TICKER_h

#include <LoLaClock\IClock.h>
#include <RTClock.h>

class ClockTicker : public virtual IClockTickSource
{
private:
	RTClock RTCInstance;
	static const rtc_clk_src ClockSourceType = RTCSEL_LSI;

public:
	ClockTicker();

	bool Setup(ISyncedClock* target);

	virtual void SetupTrainingTickInterrupt();

	virtual void SetupTickInterrupt();

	virtual void DetachAll()
	{
		RTCInstance.detachAlarmInterrupt();
		RTCInstance.detachSecondsInterrupt();
	}

	virtual void SetTuneOffset(const int32_t offsetMicros) {}
};
#endif