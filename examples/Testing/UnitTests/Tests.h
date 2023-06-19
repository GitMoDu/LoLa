// Tests.h

#ifndef _TESTS_h
#define _TESTS_h


#define LOLA_UNIT_TESTING

#include <ILoLaInclude.h>


template<uint32_t HopPeriod, uint32_t Result>
const bool TestTimedChannelHopper()
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

template<bool IsFullDuplex, uint32_t StartIn, uint32_t EndIn, uint32_t Period, uint32_t Duration>
const bool TestDuplex(IDuplex* duplex)
{
	if (IsFullDuplex)
	{
		if (duplex->GetRange() != IDuplex::DUPLEX_FULL)
		{
			Serial.println(F("Duplex is expected to be full."));
			return false;
		}

		for (uint32_t i = 0; i < Period; i++)
		{
			if (!duplex->IsInRange(i, i + Duration))
			{
				Serial.print(F("Slot negative at timestamp "));
				Serial.println(i);

				return false;
			}
		}
	}
	else
	{
		if (duplex->GetRange() == IDuplex::DUPLEX_FULL)
		{
			Serial.println(F("Duplex is expected to not be full."));
			return false;
		}

		for (uint32_t i = 0; i < StartIn; i++)
		{
			if (duplex->IsInRange(i, i + Duration))
			{
				Serial.print(F("Slot false pre-positive at timestamp "));
				Serial.println(i);

				return false;
			}
		}

		for (uint32_t i = StartIn; i < EndIn - Duration; i++)
		{
			if (!duplex->IsInRange(i, i + Duration))
			{
				Serial.print(F("Slot false negative at timestamp "));
				Serial.println(i);

				return false;
			}
		}

		for (uint32_t i = EndIn - Duration; i <= EndIn; i++)
		{
			if (duplex->IsInRange(i, i + Duration))
			{
				Serial.print(F("Slot duration false positive at timestamp "));
				Serial.println(i);

				return false;
			}
		}

		for (uint32_t i = EndIn + 1; i < Period; i++)
		{
			if (duplex->IsInRange(i, i + Duration))
			{
				Serial.print(F("Slot false post-positive at timestamp "));
				Serial.println(i);

				return false;
			}
		}
	}

	return true;
}

#endif

