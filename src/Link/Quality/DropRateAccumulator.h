// DropRateAccumulator.h

#ifndef _DROP_RATE_ACCUMULATOR_h
#define _DROP_RATE_ACCUMULATOR_h

#include "QualityFilters.h"

template<const uint8_t FilterScale,
	const uint8_t DropRateReference>
class AbstractDropRateAccumulator
{
private:
	EmaFilter16<FilterScale> DropRateFilter{};

protected:
	uint16_t Accumulator = 0;
	uint16_t LoopingCounter = 0;

public:
	AbstractDropRateAccumulator() {}

public:
	void Clear()
	{
		Accumulator = 0;
		LoopingCounter = 0;
		DropRateFilter.Clear(0);
	}

	const uint8_t GetQuality()
	{
		const uint16_t dropRate = GetDropRate();

		if (dropRate >= DropRateReference)
		{
			return 0;
		}
		else
		{
			return UINT8_MAX - (((uint32_t)dropRate * UINT8_MAX) / DropRateReference);
		}
	}

	/// <summary>
	/// Filtered drop rate.
	/// </summary>
	/// <returns>Drop rate in packets/second.</returns>
	const uint16_t GetDropRate()
	{
		return DropRateFilter.Get();
	}

	const uint16_t GetLoopingDropCount()
	{
		return LoopingCounter;
	}

	void Consume(const uint32_t accumulationPeriod)
	{
		const uint16_t accumulatedRateScaled = ((uint32_t)Accumulator * ONE_SECOND_MILLIS) / (accumulationPeriod / ONE_MILLI_MICROS);
		DropRateFilter.Step(accumulatedRateScaled);
		Accumulator = 0;
	}
};

/// <summary>
/// </summary>
/// <typeparam name="FilterScale"></typeparam>
/// <typeparam name="DropRateReference">How many drops/second is considered bad.</typeparam>
template<const uint8_t FilterScale,
	const uint8_t DropRateReference>
class RxDropAccumulator : public AbstractDropRateAccumulator<FilterScale, DropRateReference>
{
private:
	using BaseClass = AbstractDropRateAccumulator<FilterScale, DropRateReference>;

protected:
	using BaseClass::Accumulator;
	using BaseClass::LoopingCounter;

public:
	void Accumulate(const uint16_t dropCount)
	{
		if (UINT16_MAX - Accumulator > dropCount)
		{
			Accumulator += dropCount;
		}
		else
		{
			Accumulator = UINT16_MAX;
		}

		LoopingCounter += dropCount;
	}
};

/// <summary>
/// </summary>
/// <typeparam name="FilterScale"></typeparam>
/// <typeparam name="DropRateReference">How many drops/second is considered bad.</typeparam>
template<const uint8_t FilterScale,
	const uint8_t DropRateReference>
class TxDropAccumulator : public AbstractDropRateAccumulator<FilterScale, DropRateReference>
{
private:
	using BaseClass = AbstractDropRateAccumulator<FilterScale, DropRateReference>;

protected:
	using BaseClass::Accumulator;
	using BaseClass::LoopingCounter;

public:
	void Accumulate(const uint16_t loopingDropCounter)
	{
		const uint16_t delta = loopingDropCounter - LoopingCounter;
		LoopingCounter = loopingDropCounter;

		if (UINT16_MAX - Accumulator > delta)
		{
			Accumulator += delta;
		}
		else
		{
			Accumulator = UINT16_MAX;
		}
	}
};
#endif