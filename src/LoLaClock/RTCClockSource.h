// RTCClockSource.h

#ifndef _RTC_CLOCKSOURCE_h
#define _RTC_CLOCKSOURCE_h

#include <LoLaClock\ILolaClockSource.h>
#include <RTClock.h>

#include <libmaple/systick.h>


class LoLaTimerClockSource : public ILoLaClockSource
{
private:
	static const uint32_t ROLL_OVER_PERIOD_FACTOR = 3;
	static const uint32_t ROLL_OVER_PERIOD = ONE_SECOND_MICROS / ROLL_OVER_PERIOD_FACTOR;

	uint32_t MicrosPerTimerIncrement = 1;
	uint32_t TimerOverFlow = 1;
	uint32_t TimerMicros = 0;
	uint32_t RolloverIncrements = ROLL_OVER_PERIOD;

	HardwareTimer* MicrosTimer = &Timer1;

public:
	LoLaTimerClockSource() : ILoLaClockSource()
	{
	}

	void TimerInterrupt()
	{
		TimerMicros += RolloverIncrements;
	}

protected:
	//uint32_t GetMicros()
	//{
	//	asm volatile("nop"); //allow interrupt to fire
	//	asm volatile("nop");

	//	return TimerMicros + (MicrosPerTimerIncrement*MicrosTimer->getCount());
	//}

	void OnDriftUpdated(const int32_t driftMicros)
	{
		RolloverIncrements = ROLL_OVER_PERIOD + (driftMicros / ROLL_OVER_PERIOD_FACTOR);
	}

	virtual void Attach()
	{
		MicrosTimer->pause();
		MicrosTimer->setCount(0);
		TimerOverFlow = MicrosTimer->setPeriod(ROLL_OVER_PERIOD);
		MicrosPerTimerIncrement = (ROLL_OVER_PERIOD / TimerOverFlow);
		RolloverIncrements = ONE_SECOND_MICROS;
		TimerMicros = UINT32_MAX/2;
		Serial.print(F("\tTimerOverFlow: "));
		Serial.println(TimerOverFlow);
		Serial.print(F("\tMicrosPerTimerIncrement: "));
		Serial.println(MicrosPerTimerIncrement);

		MicrosTimer->resume();
	}
};

class RTCClockSource : public LoLaTimerClockSource
{
private:
	RTClock RTC;
	const rtc_clk_src ClockSourceType = RTCSEL_LSI;

protected:
	void Attach();

	uint32_t GetUTCSeconds();
	void SetUTCSeconds(const uint32_t secondsUTC);

public:
	RTCClockSource();

};
#endif