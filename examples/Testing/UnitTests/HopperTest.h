// HopperTest.h

#ifndef _HOPPERTEST_h
#define _HOPPERTEST_h

#include <Arduino.h>
#include "Tests.h"

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

class HopperTest
{
private:
	static constexpr uint32_t CHANNEL_HOP_PERIOD_MICROS = 10000;

	static const bool TestHopperTypes(Scheduler& scheduler)
	{
		NoHopNoChannel ChannelNoHop{};
		NoHopFixedChannel<1> ChannelFixedNoHop{};
		TimedChannelHopper<CHANNEL_HOP_PERIOD_MICROS> ChannelHop(scheduler);

		if (ChannelNoHop.GetHopPeriod() != IChannelHop::NOT_A_HOPPER)
		{
			Serial.println(F("NoHopNoChannel claims to be a hopper."));
			return false;
		}

		if (ChannelFixedNoHop.GetHopPeriod() != IChannelHop::NOT_A_HOPPER)
		{
			Serial.println(F("FixedChannelNoHop claims to be a hopper."));
			return false;
		}

		if (ChannelHop.GetHopPeriod() == IChannelHop::NOT_A_HOPPER)
		{
			Serial.println(F("TimedChannelHopper claims to not be a hopper."));
			return false;
		}

		return true;
	}

	static const bool TestHopperSleepPeriod()
	{
		if (!TestTimedChannelHopper<500, 0>()) { return false; }
		if (!TestTimedChannelHopper<1000, 0>()) { return false; }
		if (!TestTimedChannelHopper<2000, 1>()) { return false; }
		if (!TestTimedChannelHopper<3000, 2>()) { return false; }
		if (!TestTimedChannelHopper<5000, 4>()) { return false; }
		if (!TestTimedChannelHopper<10000, 9>()) { return false; }
		if (!TestTimedChannelHopper<15000, 14>()) { return false; }
		if (!TestTimedChannelHopper<20000, 19>()) { return false; }
		if (!TestTimedChannelHopper<25000, 24>()) { return false; }
		if (!TestTimedChannelHopper<30000, 29>()) { return false; }
		if (!TestTimedChannelHopper<40000, 39>()) { return false; }
		if (!TestTimedChannelHopper<45000, 44>()) { return false; }
		if (!TestTimedChannelHopper<50000, 49>()) { return false; }
		if (!TestTimedChannelHopper<75000, 74>()) { return false; }
		if (!TestTimedChannelHopper<100000, 99>()) { return false; }

		return true;
	}

	template<uint32_t HopPeriod, uint32_t Result>
	static const bool TestTimedChannelHopper()
	{
		if (TimedChannelHopper<HopPeriod>::GetDelayPeriod() != Result)
		{
			Serial.print(F("TestHopperSleepPeriod Fail at "));
			Serial.println(HopPeriod);
			Serial.print('\t');
			Serial.print(TimedChannelHopper<HopPeriod>::GetDelayPeriod());
			Serial.print(F("!="));
			Serial.println(Result);

			return false;
		}

		return true;
	}

public:
	static const bool RunTests(Scheduler& scheduler)
	{
		if (!TestHopperSleepPeriod())
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}

		if (!TestHopperTypes(scheduler))
		{
			Serial.println(F("TestTimestampError failed"));
			return false;
		}

		return true;
	}
};
#endif

