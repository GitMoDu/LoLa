// LinkClockTracker.h

#ifndef _LINK_CLOCK_TRACKER_h
#define _LINK_CLOCK_TRACKER_h

#include <stdint.h>
#include "LoLaLinkDefinition.h"

#include "Quality\QualityFilters.h"

class LinkServerClockTracker
{
private:
	static constexpr uint8_t ERROR_REFERENCE = LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE * 2;
	static constexpr uint8_t QUALITY_FILTER_SCALE = 220;
	static constexpr uint8_t QUALITY_COUNT = 9;

private:
	EmaFilter8<QUALITY_FILTER_SCALE> QualityFilter{};

	int32_t ReplyError = 0;
	uint8_t Accumulated = 0;

	bool ReplyPending = false;

public:
	void Reset()
	{
		QualityFilter.Clear();
		Accumulated = 0;
		ReplyPending = false;
	}

	void OnReplySent()
	{
		ReplyPending = false;
	}

	const uint8_t GetQuality()
	{
		if (Accumulated >= QUALITY_COUNT)
		{
			return QualityFilter.Get();
		}
		else
		{
			return ((uint16_t)QualityFilter.Get() * Accumulated) / QUALITY_COUNT;
		}
	}

	const bool HasReplyPending()
	{
		return ReplyPending;
	}

	const int32_t GetLinkedReplyError()
	{
		return ReplyError;
	}

	void OnLinkedEstimateReceived(const uint32_t real, const uint32_t estimate)
	{
		ReplyPending = true;
		ReplyError = real - estimate;

		QualityFilter.Step(GetErrorQuality(ReplyError));

		if (Accumulated < UINT8_MAX)
		{
			Accumulated++;
		}
	}

private:
	const uint8_t GetErrorQuality(const int16_t error)
	{
		int16_t absError = error;
		if (absError < 0)
		{
			absError = -absError;
		}

		if ((int32_t)absError >= (int32_t)ERROR_REFERENCE)
		{
			return 0;
		}
		else
		{
			return UINT8_MAX - (((uint32_t)absError * UINT8_MAX) / ERROR_REFERENCE);
		}
	}
};

class LinkClientClockTracker
{
private:
	static constexpr uint32_t CLOCK_TUNE_PERIOD = 2200000;
	static constexpr uint32_t CLOCK_TUNE_BASE_PERIOD = 400000;

	static constexpr uint8_t ERROR_REFERENCE = LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE * 2;
	static constexpr uint8_t DEVIATION_REFERENCE = LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE / 4;

	static constexpr uint8_t CLOCK_TUNE_RETRY_DUPLEX_COUNT = 8;
	static constexpr uint8_t CLOCK_SYNC_SAMPLE_COUNT = 3;

	static constexpr uint8_t CLOCK_FILTER_SCALE = 10;
	static constexpr uint8_t CLOCK_TUNE_RATIO = 32;
	static constexpr uint8_t CLOCK_REJECT_DEVIATION = 3;

	static constexpr uint8_t QUALITY_FILTER_SCALE = 200;
	static constexpr uint8_t QUALITY_COUNT = 8 * CLOCK_SYNC_SAMPLE_COUNT;

	enum class ClockTuneStateEnum
	{
		Idling,
		Sending,
		WaitingForReply,
		ResultReady
	} ClockTuneState = ClockTuneStateEnum::Idling;

private:
	const uint32_t ClockTuneRetryPeriod;
	const uint32_t ClockTuneMinPeriod;

private:
	EmaFilter8<QUALITY_FILTER_SCALE> QualityFilter{};

	int16_t ErrorSamples[CLOCK_SYNC_SAMPLE_COUNT]{};

	uint32_t LastClockSent = 0;
	uint32_t LastClockSync = 0;

	int32_t TuneError = 0;
	int32_t AverageError = 0;
	uint16_t DeviationError = 0;

	uint8_t Accumulated = 0;
	uint8_t SampleCount = 0;

public:
	LinkClientClockTracker(const uint16_t duplexPeriod)
		: ClockTuneRetryPeriod(((uint32_t)duplexPeriod* CLOCK_TUNE_RETRY_DUPLEX_COUNT))
		, ClockTuneMinPeriod(CLOCK_TUNE_BASE_PERIOD + (ClockTuneRetryPeriod * CLOCK_SYNC_SAMPLE_COUNT))
	{}

	void Reset()
	{
		QualityFilter.Clear();
		LastClockSent = 0;
		LastClockSync = 0;
		TuneError = 0;
		AverageError = 0;
		DeviationError = 0;
		Accumulated = 0;
		SampleCount = 0;
	}

	const int16_t GetResultCorrection()
	{
		return AverageError / CLOCK_FILTER_SCALE;
	}

	void Reset(const uint32_t timestamp)
	{
		ClockTuneState = ClockTuneStateEnum::Idling;
		LastClockSync = timestamp - CLOCK_TUNE_PERIOD;
		LastClockSent = timestamp;
		AverageError = 0;
		DeviationError = 0;
		TuneError = 0;
		QualityFilter.Clear(0);
	}

	const uint8_t GetQuality()
	{
		if (Accumulated >= QUALITY_COUNT)
		{
			return QualityFilter.Get();
		}
		else
		{
			return ((uint16_t)QualityFilter.Get() * Accumulated) / QUALITY_COUNT;
		}
	}

	const bool HasResultReady()
	{
		return ClockTuneState == ClockTuneStateEnum::ResultReady;
	}

	const bool HasRequestToSend(const uint32_t timestamp)
	{
		switch (ClockTuneState)
		{
		case ClockTuneStateEnum::Idling:
			return (timestamp - LastClockSync) > GetSyncPeriod(GetQuality());
			break;
		case ClockTuneStateEnum::Sending:
		case ClockTuneStateEnum::WaitingForReply:
			return (timestamp - LastClockSent) > ClockTuneRetryPeriod;
			break;
		default:
			return false;
			break;
		}
	}

	void OnReplyReceived(const uint32_t timestamp, const int32_t error)
	{
		switch (ClockTuneState)
		{
		case ClockTuneStateEnum::Idling:
		case ClockTuneStateEnum::Sending:
		case ClockTuneStateEnum::ResultReady:
			return; // Unexpected reply.
		default:
			break;
		}

		int16_t cappedError = 0;
		if (error > INT16_MAX)
		{
			cappedError = INT16_MAX;
		}
		else if (error < INT16_MIN)
		{
			cappedError = INT16_MIN;
		}
		else
		{
			cappedError = error;
		}

		StepError(cappedError);
		if (Accumulated < UINT8_MAX)
		{
			Accumulated++;
		}

		if (SampleCount >= CLOCK_SYNC_SAMPLE_COUNT)
		{
			LastClockSync = timestamp;
			UpdateErrorsAverage();

			if (DeviationError > CLOCK_REJECT_DEVIATION)
			{
				ReplaceAverageWithBest();
			}

			UpdateTuneError();
			ClockTuneState = ClockTuneStateEnum::ResultReady;
		}
		else
		{
			ClockTuneState = ClockTuneStateEnum::Sending;
		}
	}

	void OnRequestSent(const uint32_t timestamp)
	{
		switch (ClockTuneState)
		{
		case ClockTuneStateEnum::Idling:
			SampleCount = 0;
			break;
		default:
			break;
		}

		ClockTuneState = ClockTuneStateEnum::WaitingForReply;
		LastClockSent = timestamp;
	}

	const bool WaitingForClockReply()
	{
		return ClockTuneState == ClockTuneStateEnum::WaitingForReply;
	}

	const int16_t ConsumeTuneShiftMicros()
	{
		const int16_t tuneMicros = TuneError / CLOCK_FILTER_SCALE;

		TuneError -= tuneMicros * CLOCK_FILTER_SCALE;

		return tuneMicros;
	}

	void OnResultRead()
	{
		switch (ClockTuneState)
		{
		case ClockTuneStateEnum::ResultReady:
			ClockTuneState = ClockTuneStateEnum::Idling;
			break;
		default:
			break;
		}
	}

private:
	const uint32_t GetSyncPeriod(const uint8_t quality)
	{
		return (uint32_t)ClockTuneMinPeriod + (((uint32_t)(CLOCK_TUNE_PERIOD - ClockTuneMinPeriod) * quality) / UINT8_MAX);
	}

	void StepError(const int16_t error)
	{
		ErrorSamples[SampleCount] = error;
		SampleCount++;
	}

	void UpdateTuneError()
	{
		TuneError += ((AverageError - TuneError) * CLOCK_TUNE_RATIO) / UINT8_MAX;
	}

	void UpdateErrorsAverage()
	{
		int32_t sum = 0;
		for (uint_fast8_t i = 0; i < CLOCK_SYNC_SAMPLE_COUNT; i++)
		{
			sum += (int32_t)ErrorSamples[i] * CLOCK_FILTER_SCALE;
		}
		AverageError = sum / CLOCK_SYNC_SAMPLE_COUNT;

		const int16_t average = AverageError / CLOCK_FILTER_SCALE;
		uint32_t deviationSum = 0;
		for (uint_fast8_t i = 0; i < CLOCK_SYNC_SAMPLE_COUNT; i++)
		{
			const int16_t delta = ErrorSamples[i] - average;
			if (delta >= 0)
			{
				deviationSum += delta;
			}
			else
			{
				deviationSum += -delta;
			}
		}
		DeviationError = deviationSum / CLOCK_SYNC_SAMPLE_COUNT;

		const uint8_t errorQuality = GetErrorQuality();
		const uint8_t deviationQuality = GetDeviationQuality();

		if (deviationQuality > errorQuality)
		{
			QualityFilter.Step(deviationQuality);
		}
		else
		{
			QualityFilter.Step(errorQuality);
		}
	}

	void ReplaceAverageWithBest()
	{
		int16_t bestError = INT16_MAX;
		for (uint_fast8_t i = 0; i < CLOCK_SYNC_SAMPLE_COUNT; i++)
		{
			int16_t error = ErrorSamples[i];
			if (error < 0)
			{
				error = -error;
			}

			if (error < bestError)
			{
				bestError = error;
			}
		}

		AverageError = ((int32_t)bestError * CLOCK_FILTER_SCALE) / 2;
	}

	const uint8_t GetErrorQuality()
	{
		int16_t absError = GetResultCorrection();
		if (absError < 0)
		{
			absError = -absError;
		}

		if ((int32_t)absError >= (int32_t)ERROR_REFERENCE)
		{
			return 0;
		}
		else
		{
			return UINT8_MAX - (((uint32_t)absError * UINT8_MAX) / ERROR_REFERENCE);
		}
	}

	const uint8_t GetDeviationQuality()
	{
		if (DeviationError >= DEVIATION_REFERENCE)
		{
			return 0;
		}
		else
		{
			return UINT8_MAX - (((uint32_t)DeviationError * UINT8_MAX) / DEVIATION_REFERENCE);
		}
	}

#if defined(LOLA_DEBUG_LINK_CLOCK)
public:
	void DebugClockError()
	{
		Serial.println(F("Client Clock Sync"));
		Serial.print(F("\tQuality "));
		Serial.println(GetQuality());
		Serial.print(F("\tSync Period "));
		Serial.print(GetSyncPeriod(GetQuality()) / 1000);
		Serial.println(F(" ms"));
		Serial.print(F("\tAverageError "));
		Serial.println(AverageError / CLOCK_FILTER_SCALE);
		Serial.print(F("\tDeviation "));
		Serial.println(DeviationError);
	}
#endif
};
#endif