// LinkClock.h

#ifndef _LINK_CLOCK_TRACKER_h
#define _LINK_CLOCK_TRACKER_h

#include <stdint.h>
#include "LoLaLinkDefinition.h"

class LinkServerClockTracker
{
private:
	int32_t ReplyError = 0;
	bool ReplyPending = false;

public:
	void Reset()
	{
		ReplyPending = false;
	}

	void OnReplySent()
	{
		ReplyPending = false;
	}

	const uint8_t GetQuality()
	{
		//TODO:
		return UINT8_MAX / 2;
	}

	const bool HasReplyPending()
	{
		return ReplyPending;
	}

	const int32_t GetLinkedReplyError()
	{
		return ReplyError;
	}

	void OnLinkedEstimateReceived(const uint32_t estimate, const uint32_t real)
	{
		ReplyPending = true;
		ReplyError = real - estimate;
	}
};

class LinkClientClockTracker
{
private:
	static constexpr uint32_t CLOCK_TUNE_PERIOD = 1200;
	static constexpr uint32_t CLOCK_TUNE_RETRY_PERIOD = 87;

	static constexpr uint8_t CLOCK_SYNC_SAMPLE_COUNT = 3;
	static constexpr uint8_t CLOCK_FILTER_SCALE = 10;
	static constexpr uint8_t CLOCK_TUNE_RATIO = 32;
	static constexpr uint8_t CLOCK_REJECT_DEVIATION = 10;

	enum class ClockTuneStateEnum
	{
		Idling,
		Sending,
		WaitingForReply,
		ResultReady
	} ClockTuneState = ClockTuneStateEnum::Idling;

private:
	int32_t TuneError = 0;
	uint16_t DeviationError = 0;

private:
	int32_t AverageError = 0;

	uint32_t LastClockSync = 0;
	uint32_t LastClockSent = 0;

	uint32_t RequestSendDuration = 0;

	int16_t ErrorSamples[CLOCK_SYNC_SAMPLE_COUNT]{};
	uint8_t SampleCount = 0;

public:
	void SetRequestSendDuration(const uint32_t requestSendDuration)
	{
		RequestSendDuration = requestSendDuration;
	}

	const uint32_t GetRequestSendDuration()
	{
		return RequestSendDuration;
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
	}

	const uint8_t GetQuality()
	{
		//TODO:
		return UINT8_MAX / 2;
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
			return (timestamp - LastClockSync) > CLOCK_TUNE_PERIOD;
			break;
		case ClockTuneStateEnum::Sending:
		case ClockTuneStateEnum::WaitingForReply:
			return (timestamp - LastClockSent) > CLOCK_TUNE_RETRY_PERIOD;
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

		ClockTuneState = ClockTuneStateEnum::Sending;

		if ((error >= 0 && error < LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS)
			|| (error < 0 && error > -(int32_t)LoLaLinkDefinition::CLOCK_TUNE_RANGE_MICROS))
		{
			StepError(error);

			if (SampleCount >= CLOCK_SYNC_SAMPLE_COUNT)
			{
				LastClockSync = timestamp;
				UpdateErrorsAverage();

				if (DeviationError > CLOCK_REJECT_DEVIATION)
				{
#if defined(DEBUG_LOLA)
					Serial.print(F("Clock Sync jitter error was big: "));
					Serial.print(DeviationError / CLOCK_FILTER_SCALE);
#endif
					ReplaceAverageWithBest();
					UpdateTuneError();
#if defined(DEBUG_LOLA)
					Serial.print(F(" us. Replacing with best error: "));
					Serial.println(AverageError / CLOCK_FILTER_SCALE);
#endif
				}
				else
				{
					UpdateTuneError();
				}
				ClockTuneState = ClockTuneStateEnum::ResultReady;
			}
		}
#if defined(DEBUG_LOLA)
		else {
			Serial.print(F("Clock Sync error too big: "));
			Serial.print(error);
			Serial.println(F(" us."));
		}
#endif
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
	}

	void ReplaceAverageWithBest()
	{
		const int16_t average = AverageError / CLOCK_FILTER_SCALE;

		int16_t bestError = 0;
		int16_t bestDeviation = INT16_MAX;
		for (uint_fast8_t i = 0; i < CLOCK_SYNC_SAMPLE_COUNT; i++)
		{
			int16_t delta = ErrorSamples[i] - average;
			if (delta < 0)
			{
				delta = -delta;
			}

			if (delta < bestDeviation)
			{
				bestError = ErrorSamples[i];
				bestDeviation = delta;
			}
		}

		AverageError = (int32_t)bestError * CLOCK_FILTER_SCALE;
	}

#if defined(LOLA_DEBUG_LINK_CLOCK)
public:
	void DebugClockError()
	{
		Serial.print(AverageError);
		Serial.print('\t');
		Serial.print(DeviationError);
	}
#endif
};
#endif