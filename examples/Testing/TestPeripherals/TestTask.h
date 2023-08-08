// TestTask.h

#ifndef _TEST_TASK_h
#define _TEST_TASK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>



/// <summary>
/// </summary>
template<const uint8_t SecondsTestSize, const uint16_t MaxDeltaMicros>
class TestTask : private Task
	, public virtual IClockSource::IClockListener
{
private:
	uint32_t History[SecondsTestSize + 1]{};

	uint32_t Timeout = 0;


	IClockSource* ClockSource = nullptr;

	void (*ClockTestComplete)(const bool success) = nullptr;

	enum TestStateEnum
	{
		NoTest,
		ClockTesting
	};

	enum ClockSourceTestingStageEnum
	{
		TicklessStartingClock,
		TicklessCounting,
		TickedStartingClock,
		TickedCounting
	};

	TestStateEnum TestState = TestStateEnum::NoTest;

	ClockSourceTestingStageEnum ClockTestingStage = ClockSourceTestingStageEnum::TicklessStartingClock;

	volatile uint_fast8_t TickCount = 0;


public:
	TestTask(Scheduler& scheduler)
		: IClockSource::IClockListener()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
	}

public:
	virtual void OnClockTick()
	{
		if (ClockTestingStage == ClockSourceTestingStageEnum::TickedCounting)
		{
			History[TickCount] = micros();
			TickCount++;
		}
	}

public:
	virtual bool Callback() final
	{
		switch (TestState)
		{
		case TestStateEnum::NoTest:
			Task::disable();
			break;
		case TestStateEnum::ClockTesting:
			RunClockTesting();
			Task::enable();
			break;
		default:
			TestState = TestStateEnum::NoTest;
			Task::enable();
			break;
		}

		return true;
	}


public:
	const bool StartClockTestTask(IClockSource* clockSource, void (*clockTestComplete)(const bool success))
	{
		ClockSource = clockSource;
		ClockTestComplete = clockTestComplete;

		if (ClockTestComplete != nullptr && SecondsTestSize > 1 && SecondsTestSize < 11)
		{
			TestState = TestStateEnum::ClockTesting;
			if (ClockSource->ClockHasTick())
			{
				ClockTestingStage = ClockSourceTestingStageEnum::TickedStartingClock;
			}
			else
			{
				ClockTestingStage = ClockSourceTestingStageEnum::TicklessStartingClock;
			}
			Task::enable();

			return true;
		}

		return false;
	}

private:

	void RunClockTesting()
	{
		switch (ClockTestingStage)
		{
		case ClockSourceTestingStageEnum::TicklessStartingClock:
			if (!ClockSource->StartClock(this, 0))
			{
				Serial.println(F("Clock failed to start."));
				TestState = TestStateEnum::NoTest;
				ClockTestComplete(false);
				Task::enable();
			}
			else
			{
				ClockTestingStage = ClockSourceTestingStageEnum::TicklessCounting;
				Task::delay(1000);
			}
			break;
		case ClockSourceTestingStageEnum::TicklessCounting:
			ClockSource->StopClock();
			TestState = TestStateEnum::NoTest;
			ClockTestComplete(true);
			Task::enable();
			break;
		case ClockSourceTestingStageEnum::TickedStartingClock:
			if (!ClockSource->StartClock(this, 0))
			{
				Serial.println(F("Clock failed to start."));
				TestState = TestStateEnum::NoTest;
				ClockTestComplete(false);
			}
			else
			{
				Serial.println(F("TickedStartingClock"));
				Timeout = micros() + (1100000L * SecondsTestSize);
				TickCount = 0;
				ClockTestingStage = ClockSourceTestingStageEnum::TickedCounting;
			}
			Task::enable();
			break;
		case ClockSourceTestingStageEnum::TickedCounting:
			Serial.println(F("TODO: ClockHasTick test."));
			//TestState = TestStateEnum::NoTest;
			//ClockSource->StopClock();
			//ClockTestComplete(false);
			//return;

			if (TickCount > SecondsTestSize)
			{
				TestState = TestStateEnum::NoTest;
				ClockSource->StopClock();
				ClockTestComplete(EvaluateClockResult());
			}
			else if (micros() > Timeout)
			{
				TestState = TestStateEnum::NoTest;
				ClockTestComplete(false);


				Serial.println(F("Clock Error, timed out."));
			}
			break;
		default:
			TestState = TestStateEnum::NoTest;
			break;
		}
	}

private:
	const bool EvaluateClockResult()
	{
		uint64_t avg = 0;
		for (size_t i = 1; i < SecondsTestSize + 1; i++)
		{
			avg += History[i] - History[i - 1];
		}
		const uint32_t microsAverage = avg / SecondsTestSize;


		uint32_t highestDelta = 0;

		for (size_t i = 1; i < SecondsTestSize + 1; i++)
		{
			const uint32_t oneSecond = History[i] - History[i - 1];

			if (abs((int32_t)(oneSecond - 1000000L)) > MaxDeltaMicros)
			{
				Serial.print(F("Clock Error too large: "));
				Serial.print((int32_t)(oneSecond - 1000000L));
				Serial.println(F("us"));

				return false;
			}

			const int32_t delta = oneSecond - microsAverage;

			if (abs(delta) > highestDelta)
			{
				highestDelta = abs(delta);
			}

			if (abs(delta) > MaxDeltaMicros)
			{
				Serial.print(F("Timer Deviation too large: "));
				Serial.print(delta);
				Serial.println(F("us"));

				return false;
			}
		}

		Serial.print(F(" Average ("));
		Serial.print(SecondsTestSize);
		Serial.print(F(" samples) : "));
		Serial.print(microsAverage);
		Serial.print(F("us for 1 second. Highest deviation: "));
		Serial.print(highestDelta);
		Serial.print(F("us of "));
		Serial.print(MaxDeltaMicros);
		Serial.println(F("us"));


		return true;
	}
};

//template<const uint8_t SecondsTestSize, const uint16_t MaxDeltaMicros>
//const bool TestTimerSource(ITimerSource* timerSource, const String& label)
//{
//	if (timerSource == nullptr)
//	{
//		Serial.println(F("TimerSource is null."));
//		return false;
//	}
//
//	if (SecondsTestSize < 2) {
//		Serial.println(F("SecondsTestSize too small: "));
//		return false;
//	}
//
//	uint32_t history[SecondsTestSize + 1]{};
//
//	timerSource->StartTimer();
//
//	delayMicroseconds(123456);
//
//	uint_fast8_t i = 0;
//	uint32_t start = micros() - 1000000L;
//	while (i < SecondsTestSize + 1)
//	{
//		if (micros() - start >= 1000000L)
//		{
//			start = micros();
//			history[i] = timerSource->GetCounter();
//			i++;
//		}
//	}
//	timerSource->StopTimer();
//
//	uint64_t avg = 0;
//	for (size_t i = 1; i < SecondsTestSize + 1; i++)
//	{
//		avg += history[i] - history[i - 1];
//	}
//	avg /= SecondsTestSize;
//
//	const uint32_t microsAverage = (avg * 1000000L) / timerSource->GetDefaultRollover();
//
//	uint32_t highestDelta = 0;
//
//	for (size_t i = 1; i < SecondsTestSize + 1; i++)
//	{
//		const uint32_t oneSecond = ((uint64_t)(history[i] - history[i - 1]) * 1000000L) / timerSource->GetDefaultRollover();
//
//		if (abs((int32_t)(oneSecond - 1000000L)) > MaxDeltaMicros)
//		{
//			Serial.print(F("Timer Error too large: "));
//			Serial.print((int32_t)(oneSecond - 1000000L));
//			Serial.println(F("us"));
//
//			return false;
//		}
//
//		const int32_t delta = oneSecond - microsAverage;
//
//		if (abs(delta) > highestDelta)
//		{
//			highestDelta = abs(delta);
//		}
//
//		if (abs(delta) > MaxDeltaMicros)
//		{
//			Serial.print(F("Timer Deviation too large: "));
//			Serial.print(delta);
//			Serial.println(F("us"));
//
//			return false;
//		}
//	}
//
//	Serial.print(label);
//	Serial.print(F(" Average ("));
//	Serial.print(SecondsTestSize);
//	Serial.print(F(" samples) : "));
//	Serial.print(microsAverage);
//	Serial.print(F("us for 1 second. Highest deviation: "));
//	Serial.print(highestDelta);
//	Serial.print(F("us of "));
//	Serial.print(MaxDeltaMicros);
//	Serial.println(F("us"));
//
//	return true;
//}

#endif
