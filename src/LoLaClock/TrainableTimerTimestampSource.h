// TrainableTimerTimestampSource.h

#ifndef _TRAINABLETIMER_TIMESTAMP_SOURCE_h
#define _TRAINABLETIMER_TIMESTAMP_SOURCE_h

#include <LoLaClock\IClock.h>
#include <LoLaClock\FreeRunningTimer.h>


class TrainableTimerSource
	: public virtual IClockMicrosSource
{
private:
	enum TrainingStepEnum : uint8_t
	{
		WaitingForFirstTick = 0,
		WaitingForSecondTick = 1,
		TrainingDone = 2
	};

	TrainingStepEnum TrainingStep = TrainingStepEnum::WaitingForFirstTick;

	// Training counts.
	const static uint8_t SetupSampleCount = 2;
	const static uint8_t TuneSampleCount = 4;


	// Static helpers.
	uint8_t TargetSamples = 0;
	volatile uint32_t Step = 0;
	volatile uint32_t TickStep = 0;

	// Training data.
	uint64_t AverageHelper = 0;
	uint8_t SampleCount = 0;

protected:
	FreeRunningTimer HardwareTimer;

	// Runtime calculated value.
	volatile uint32_t TickSteps = 0;

private:
	IClockTickSource* TickSource = nullptr;

public:
	TrainableTimerSource()
		: IClockMicrosSource()
		, HardwareTimer()
	{
	}

	bool Setup(IClockTickSource* trainerTickSource)
	{
		TickSource = trainerTickSource;

		if (TickSource != nullptr &&
			!HardwareTimer.SetupTimer())
		{
#ifdef DEBUG_LOLA
			Serial.println(F("Hardware timer setup failed."));
#endif
			return false;
		}

		StartTraining(SetupSampleCount);

		return true;
	}

	virtual void OnTuneUpdated()
	{
		StartTraining(TuneSampleCount);
	}

	virtual uint32_t GetMicros()
	{
		return ((uint64_t)HardwareTimer.GetCurrentStep() * ONE_SECOND_MICROS) / TickSteps;
	}

	bool HasFirstTrain()
	{
		return TickSteps > 0;
	}
	
	// Returns current micros.
	uint32_t OnTrainingTick()
	{
		Step = HardwareTimer.GetCurrentStep();

		switch (TrainingStep)
		{
		case TrainingStepEnum::WaitingForFirstTick:
			AverageHelper = 0;
			TickStep = Step;
			TrainingStep = TrainingStepEnum::WaitingForSecondTick;
			break;
		case TrainingStepEnum::WaitingForSecondTick:
			AverageHelper += Step - TickStep;
			SampleCount++;
			if (SampleCount < TargetSamples)
			{
				TickStep = Step;
			}
			else
			{
				TrainingStep = TrainingStepEnum::TrainingDone;
			}
			break;
		case TrainingStepEnum::TrainingDone:
			TickSteps = AverageHelper / SampleCount;
			TickSource->SetupTickInterrupt();
			break;
		default:
			TickSource->SetupTickInterrupt();
			break;
		}

		return (((uint64_t)Step * ONE_SECOND_MICROS) / TickSteps) % UINT32_MAX;
	}

private:
	void StartTraining(uint8_t targetSamples = SetupSampleCount)
	{
		TargetSamples = targetSamples;
		AverageHelper = 0;
		SampleCount = 0;
		TrainingStep = TrainingStepEnum::WaitingForFirstTick;
		TickSource->SetupTrainingTickInterrupt();
	}
};

class TickTrainedTimerSyncedClockWithCallback : public TrainableTimerSource
{
public:
	TickTrainedTimerSyncedClockWithCallback() :TrainableTimerSource() {}

	virtual void SetCallbackTarget(ISyncedCallbackTarget* source)
	{
		// Just pass it along.
		HardwareTimer.SetCallbackTarget(source);
	}

	virtual void StartCallbackAfterMicros(const uint32_t delayMicros)
	{
		HardwareTimer.StartCallbackAfterSteps(delayMicros / TickSteps);
	}
};
#endif