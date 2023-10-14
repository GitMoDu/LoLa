// RssiAccumulator.h

#ifndef _RSSI_ACCUMULATOR_h
#define _RSSI_ACCUMULATOR_h

#include "QualityFilters.h"

template<const uint8_t FilterScale>
class RssiAccumulator
{
protected:
	uint16_t Accumulator = 0;
	uint8_t Counts = 0;

	EmaFilter8<FilterScale> Filter{};

public:
	void Clear()
	{
		Accumulator = 0;
		Counts = 0;
		Filter.Clear(0);
	}

	void Accumulate(const uint8_t rssi)
	{
		if (Counts < UINT8_MAX)
		{
			if ((UINT16_MAX - Accumulator) >= rssi)
			{
				Counts++;
				Accumulator += rssi;
			}
		}
	}

	void Consume()
	{
		if (Counts > 0)
		{
			const uint8_t averageRssi = Accumulator / Counts;

			Filter.Step(averageRssi);
		}
		else
		{
			Filter.Step(Filter.Get());
		}
		Counts = 0;
		Accumulator = 0;
	}

	const uint8_t GetQuality()
	{
		if (Counts > 0)
		{
			const uint8_t averageRssi = Accumulator / Counts;

			return ((uint_least16_t)averageRssi + Filter.Get()) / 2;
		}
		else
		{
			return Filter.Get();
		}
	}
};
#endif