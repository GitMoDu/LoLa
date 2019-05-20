// LoLaTimerClockSource.h

#ifndef _LOLATIMERCLOCKSOURCE_h
#define _LOLATIMERCLOCKSOURCE_h

#include <LoLaDefinitions.h>
#include <LoLaClock\ILolaClockSource.h>
#include <LoLaClock\FreeRunningTimer.h>
#include <libmaple/systick.h>

#define timer_ratio(a) ((a*10)/1) //1.2 Times gives around 200 milliseconds of max error.

class LoLaTimerClockSource : public ILoLaClockSource
{
private:
	static const uint32_t ROLL_OVER_PERIOD = timer_ratio(ONE_SECOND_MICROS);

	uint32_t TimerOverFlow = 1;

	uint32_t MicrosPerTimerIncrement = 1;

protected:
	HardwareTimer* MicrosTimer = &Timer1;

public:
	LoLaTimerClockSource() : ILoLaClockSource()
	{
	}

	void TimerInterrupt()
	{
		TimerOverFlow++;
	}

protected:
	uint32_t GetTick()
	{
		return (MicrosTimer->getCount() + (TimerOverFlow * UINT16_MAX)) * MicrosPerTimerIncrement;
	}

	void OnDriftUpdated(const int32_t driftMicros)
	{
		//TODO: Slow down timer proportionally.
		//MicrosPerTimerIncrement = ((ROLL_OVER_PERIOD + timer_ratio(driftMicros)) / TimerOverFlow);
	}

	virtual void Attach()
	{
		MicrosTimer->pause();
		MicrosTimer->setCount(0);
		SetTimer(ROLL_OVER_PERIOD);
		MicrosTimer->resume();

#ifdef LOLA_DEBUG_CLOCK_SYNC
		Serial.print(F("\tTimerOverFlow: "));
		Serial.println(TimerOverFlow);
		Serial.print(F("\tMicrosPerTimerIncrement: "));
		Serial.println(MicrosPerTimerIncrement);
#endif
	}

private:
	inline void SetTimer(const uint32_t rolloverPeriod)
	{
		TimerOverFlow = MicrosTimer->setPeriod(rolloverPeriod);
		MicrosPerTimerIncrement = (rolloverPeriod / TimerOverFlow);
	}
};

#endif

