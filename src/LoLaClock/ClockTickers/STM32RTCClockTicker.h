// STM32RTCClockTicker.h

#ifndef _STM32_RTC_CLOCK_TICKER_h
#define _STM32_RTC_CLOCK_TICKER_h

#include <LoLaClock\IClock.h>
#include <RTClock.h>

class ClockTicker : public virtual IClockTickSource
{
private:
	RTClock RTC;
	static const rtc_clk_src ClockSourceType = RTCSEL_LSE;

public:
	ClockTicker();

	bool Setup(IClockTickTarget* target);

	virtual void SetupTrainingTickInterrupt();

	virtual void SetupTickInterrupt();

	virtual void DetachAll()
	{
		//RTC.detach();//TODO:
	}

	// tuneOffset in ppb (Parts per Billion)
	virtual void SetTuneOffset(const int32_t tunePPB) {}
};
#endif