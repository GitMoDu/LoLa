// ClockErrorStatistics.h

#ifndef _CLOCKERRORSTATISTICS_h
#define _CLOCKERRORSTATISTICS_h

#include <RingBufCPP.h>

#include <Services\Link\LoLaLinkDefinitions.h>

class ClockErrorSample
{
public:
	int32_t ErrorMicros = 0;
	uint32_t PeriodMillis = 0;

public:
	ClockErrorSample() {}
	void Set(const int32_t absoluteErrorMicros, const uint32_t periodMillis)
	{
		ErrorMicros = (int32_t)(((int64_t)absoluteErrorMicros) / ((int64_t)(periodMillis / 1000)));
	}
};

template<const uint8_t ERROR_SAMPLE_COUNT>
class ClockErrorStatistics
{
public:
	ClockErrorSample Grunt;

	static const size_t MIN_ELEMENTS = min(LOLA_LINK_SERVICE_LINKED_CLOCK_MIN_SAMPLES, ERROR_SAMPLE_COUNT);
	static const uint32_t MAX_CLOCK_ERROR_MICROS = LOLA_LINK_SERVICE_LINKED_CLOCK_OK_ERROR_MICROS;

	RingBufCPP<ClockErrorSample, ERROR_SAMPLE_COUNT> TuneErrorSamples;

	bool NeedsUpdate = 0;

	//Statistics values.
	int32_t AverageErrorMicros = 0;
	uint32_t MaxErrorMicros = 0;

	uint8_t Count = 0;



public:
	void AddTuneSample(const int32_t errorMicros, const uint32_t periodMillis)
	{
		Grunt.ErrorMicros = errorMicros;
		Grunt.PeriodMillis = periodMillis;

		TuneErrorSamples.addForce(Grunt);
		NeedsUpdate = true;
	}

	bool HasEnoughSamples()
	{
		return TuneErrorSamples.numElements() > MIN_ELEMENTS;
	}

	bool HasFullSamples()
	{
		return TuneErrorSamples.isFull();
	}

	void AddTuneSample(const ClockErrorSample &errorSample)
	{
		TuneErrorSamples.addForce(errorSample);
		NeedsUpdate = true;
	}

	uint8_t GetNormalizedClockErrorQuality()
	{
		if (NeedsUpdate)
		{
			UpdateStatistics();
		}

		return (map(TuneErrorSamples.numElements(), 0, ERROR_SAMPLE_COUNT, 0, UINT8_MAX) +
			map(min(abs(AverageErrorMicros), MAX_CLOCK_ERROR_MICROS), MAX_CLOCK_ERROR_MICROS, 0, 0, UINT8_MAX) +
			map(min(MaxErrorMicros, MAX_CLOCK_ERROR_MICROS), MAX_CLOCK_ERROR_MICROS, 0, 0, UINT8_MAX)) / 3;
	}

	uint32_t GetMaxErrorMicros()
	{
		return MaxErrorMicros;
	}

	int32_t GetAverageError()
	{
		if (NeedsUpdate)
		{
			UpdateStatistics();
		}

		return AverageErrorMicros;
	}

	void Reset()
	{
		while (!TuneErrorSamples.isEmpty())
		{
			TuneErrorSamples.pull();
		}
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
			
			while (Count < TuneErrorSamples.numElements())
			{
				Grunt = *TuneErrorSamples.peek(Count);
				AverageErrorMicros += Grunt.ErrorMicros;
				if (abs(Grunt.ErrorMicros) > MaxErrorMicros)
				{
					MaxErrorMicros = abs(Grunt.ErrorMicros);
				}
				Count++;
			}

			AverageErrorMicros /= Count;

			NeedsUpdate = false;
		}
	}
};

#endif

