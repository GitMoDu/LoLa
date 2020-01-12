// TickTrainedTimerSyncedClock.h

#ifndef _TICK_TRAINED_TIMER_SYNCED_CLOCK_h
#define _TICK_TRAINED_TIMER_SYNCED_CLOCK_h

#include <LoLaClock\LoLaSyncedClock.h>

class TickTrainedTimerSyncedClock
	: public LoLaSyncedClock
{
private:
	enum TrainingStageEnum : uint8_t
	{
		WaitingForStart = 0,
		WaitingForTick = 1,
		TrainingDone = 2
	};

	volatile TrainingStageEnum TrainingStage = TrainingStageEnum::WaitingForStart;

	// Training counts.
	const static uint8_t SetupSampleCount = 30;
	const static uint8_t TuneSampleCount = 40;
	const static uint8_t DiscardSampleCount = 3;

	// Head start.
	static const uint32_t PreTrainedStepsPerSecond = 64960;

	// Static helpers.
	volatile uint32_t TrainingStep = 0;

	volatile uint8_t TargetSamples = 0;

	// Training data.
	volatile uint64_t AverageHelper = 0;
	volatile uint8_t SampleCount = 0;

	// First samples are always iffy, until the clock warms up.
	volatile uint8_t DiscardCount = 0;

	bool FirstTrainingDone = false;

public:
	TickTrainedTimerSyncedClock()
		: LoLaSyncedClock()
	{
	}

	virtual bool HasTraining()
	{
		return FirstTrainingDone;
	}

	bool Setup()
	{
		if (LoLaSyncedClock::Setup())
		{
#ifdef DEBUG_LOLA_CLOCK
			FirstTrainingDone = false;
#endif
			StepsPerSecond = PreTrainedStepsPerSecond;
			StartTraining(SetupSampleCount);

			return true;
		}

		return false;
	}

	virtual void SetCallbackTarget(ISyncedCallbackTarget* source)
	{
		// Just pass it along.
		TimerSource.SetCallbackTarget(source);
	}

	virtual void StartCallbackAfterMicros(const uint32_t delayMicros)
	{
		TimerSource.StartCallbackAfterSteps((delayMicros * StepsPerSecond) / ONE_SECOND_MICROS);
	}

	// Replaces OnTick(), while training.
	virtual void OnTrainingTick()
	{
		OnTick();

		// We then piggyback the interrupt to update the training values.
		switch (TrainingStage)
		{
		case TrainingStageEnum::WaitingForStart:
			TrainingStep = SecondsStep;
			TrainingStage = TrainingStageEnum::WaitingForTick;
			break;
		case TrainingStageEnum::WaitingForTick:
			if (DiscardCount >= DiscardSampleCount)
			{
				// We can deal with one overflow between ticks, but no more.
				if (SecondsStep < TrainingStep)
				{
					// Timer rollover (overflow).
					AverageHelper += (TimerRange - TrainingStep) + SecondsStep;
				}
				else
				{
					AverageHelper += SecondsStep - TrainingStep;
				}
				SampleCount++;

				// Update steps.
				StepsPerSecond = AverageHelper / SampleCount;
				if (SampleCount < TargetSamples)
				{

					TrainingStep = SecondsStep;
				}
				else
				{
					TrainingStage = TrainingStageEnum::TrainingDone;
				}
			}
			else
			{
				TrainingStep = SecondsStep;
				DiscardCount++;
			}
			break;
		case TrainingStageEnum::TrainingDone:
			FirstTrainingDone = true;
#ifdef DEBUG_LOLA_CLOCK
			Serial.print("Timer trained.");
			Serial.print(ONE_SECOND_NANOS / StepsPerSecond);
			Serial.println(" ns per Step: ");
			Serial.print("Steps per Second: ");
			Serial.println(StepsPerSecond);
#endif
			TickerSource.SetupTickInterrupt();
			break;
		default:
			TickerSource.SetupTickInterrupt();
			break;
		}
	}

protected:
	virtual void OnTuneUpdated()
	{
		if (TrainingStage = TrainingStageEnum::TrainingDone)
		{
			StartTraining(TuneSampleCount);
		}
	}

private:
	void StartTraining(uint8_t targetSamples = SetupSampleCount)
	{
		TargetSamples = targetSamples;
		AverageHelper = 0;
		SampleCount = 0;
		DiscardCount = 0;

		TrainingStage = TrainingStageEnum::WaitingForStart;
		TickerSource.SetupTrainingTickInterrupt();
	}
};
#endif