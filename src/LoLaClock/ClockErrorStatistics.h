// ClockErrorStatistics.h

#ifndef _CLOCKERRORSTATISTICS_h
#define _CLOCKERRORSTATISTICS_h


#include <Link\LoLaLinkDefinitions.h>

class ClockErrorSample
{
public:
	int32_t ErrorMicros = 0;
	uint32_t PeriodMillis = 0;

public:
	ClockErrorSample() {}
	ClockErrorSample(const int32_t errorMicros, const uint32_t periodMillis)
	{
		ErrorMicros = errorMicros;
		PeriodMillis = periodMillis;
	}
};

template<const uint8_t ERROR_SAMPLE_COUNT>
class ClockErrorStatistics
{
public:
	ClockErrorSample Grunt;

	static const uint32_t MAX_CLOCK_ERROR_MICROS = LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS;

	//RingBufCPP<ClockErrorSample, ERROR_SAMPLE_COUNT> TuneErrorSamples;

	bool NeedsUpdate = 0;

	//Statistics values.
	int32_t AverageErrorMicros = 0;
	int64_t AverageWeightedErrorMicros = 0;
	uint32_t WeightedTotalPeriodMillis = 0;
	uint32_t MaxErrorMicros = 0;

	uint8_t Count = 0;

public:
	void AddTuneSample(const int32_t errorMicros, const uint32_t periodMillis)
	{
		Grunt.ErrorMicros = errorMicros;
		Grunt.PeriodMillis = periodMillis;

		//TuneErrorSamples.addForce(Grunt);
		NeedsUpdate = true;
	}

	bool HasEnoughSamples()
	{
		return false;
		//return TuneErrorSamples.numElements() > (ERROR_SAMPLE_COUNT / 2);
	}

	void AddTuneSample(const ClockErrorSample &errorSample)
	{
		//TuneErrorSamples.addForce(errorSample);
		NeedsUpdate = true;
	}

	uint8_t GetNormalizedClockErrorQuality()
	{
		if (NeedsUpdate)
		{
			UpdateStatistics();
		}

		return 0;
		//return (map(TuneErrorSamples.numElements(), 0, ERROR_SAMPLE_COUNT, 0, UINT8_MAX) +
		//	map(min((uint32_t)abs(AverageErrorMicros), MAX_CLOCK_ERROR_MICROS), MAX_CLOCK_ERROR_MICROS, 0, 0, UINT8_MAX) +
		//	map(min((uint32_t)MaxErrorMicros, MAX_CLOCK_ERROR_MICROS), MAX_CLOCK_ERROR_MICROS, 0, 0, UINT8_MAX)) / 3;
	}

	int32_t GetAverageError()
	{
		if (NeedsUpdate)
		{
			UpdateStatistics();
		}

		return AverageErrorMicros;
	}

	int32_t GetWeightedAverageError()
	{
		if (NeedsUpdate)
		{
			UpdateStatistics();
		}

		return AverageWeightedErrorMicros;
	}

	uint32_t GetWeightedAverageDurationMillis()
	{
		if (NeedsUpdate)
		{
			UpdateStatistics();
		}

		return WeightedTotalPeriodMillis;
	}

	void Reset()
	{
		//while (!TuneErrorSamples.isEmpty())
		//{
		//	TuneErrorSamples.pull();
		//}
		AverageErrorMicros = 0;
	}


private:
	inline void UpdateStatistics()
	{
		if (NeedsUpdate)
		{
			Count = 0;
			MaxErrorMicros = 0;
			AverageErrorMicros = 0;
			AverageWeightedErrorMicros = 0;
			WeightedTotalPeriodMillis = 0;
			/*while (Count < TuneErrorSamples.numElements())
			{
				Grunt = *TuneErrorSamples.peek(Count);
				AverageErrorMicros += Grunt.ErrorMicros;
				AverageWeightedErrorMicros += (int64_t)Grunt.ErrorMicros * (int64_t)(Grunt.PeriodMillis);
				WeightedTotalPeriodMillis += Grunt.PeriodMillis;

				if (abs(Grunt.ErrorMicros) > MaxErrorMicros)
				{
					MaxErrorMicros = abs(Grunt.ErrorMicros);
				}
				Count++;
			}

			AverageErrorMicros /= Count;
			AverageWeightedErrorMicros = (int32_t)((int64_t)AverageWeightedErrorMicros / (int64_t)WeightedTotalPeriodMillis);*/

			NeedsUpdate = false;
		}
	}
};

#endif

