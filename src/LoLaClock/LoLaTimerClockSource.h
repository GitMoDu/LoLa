// LoLaTimerClockSource.h

#ifndef _LOLATIMERCLOCKSOURCE_h
#define _LOLATIMERCLOCKSOURCE_h


#include <LoLaClock\ILolaClockSource.h>
#include <libmaple/systick.h>


class LoLaTimerClockSource : public ILoLaClockSource
{
private:
	static const uint32_t ROLL_OVER_PERIOD_FACTOR = 2;
	static const uint32_t ROLL_OVER_PERIOD = ONE_SECOND_MICROS * ROLL_OVER_PERIOD_FACTOR;

	uint32_t TimerOverFlow = 1;

	HardwareTimer* MicrosTimer = &Timer1;

	uint32_t MicrosPerTimerIncrement = 1;

public:
	LoLaTimerClockSource() : ILoLaClockSource()
	{
	}

protected:
	uint32_t GetTick()
	{
		return MicrosTimer->getCount();
	}

	uint32_t GetElapsedMicros()
	{
		return (uint16_t)((uint16_t)GetTick() - ((uint16_t)LastTick)) * MicrosPerTimerIncrement;
	}

	void OnDriftUpdated(const int32_t driftMicros)
	{
		SetTimer(ROLL_OVER_PERIOD + (driftMicros * ROLL_OVER_PERIOD_FACTOR));
	}

	virtual void Attach()
	{
		MicrosTimer->pause();
		MicrosTimer->setCount(0);
		SetTimer(ROLL_OVER_PERIOD);
		MicrosTimer->resume();
	}

private:
	inline void SetTimer(const uint32_t rolloverPeriod)
	{
		TimerOverFlow = MicrosTimer->setPeriod(rolloverPeriod);
		MicrosPerTimerIncrement = (rolloverPeriod / TimerOverFlow);
	}
};

#endif

