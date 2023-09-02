// TestTask.h

#ifndef _TEST_TASK_h
#define _TEST_TASK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>



class TestTask : private Task
{
private:
	enum TestStateEnum
	{
		TestingMatch,
		Done
	};

	TestStateEnum TestState = TestStateEnum::Done;


	enum MatchStateEnum
	{
		MatchStart,
		MatchCompare,
		MatchSuccess,
		MatchFail
	};
	int32_t MatchOffset = 0;
	int32_t MatchError = 0;


	uint32_t MatchStartTimestamp = 0;

	CycleClock* MatchReference = nullptr;
	CycleClock* MatchTest = nullptr;
	void (*MatchTestComplete)(const bool success) = nullptr;

	MatchStateEnum MatchState = MatchStateEnum::MatchStart;


public:
	TestTask(Scheduler& scheduler)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
	}


public:
	virtual bool Callback() final
	{
		switch (TestState)
		{
		case TestStateEnum::TestingMatch:
			if (RunTestMicrosMatchCycles())
			{
				TestState = TestStateEnum::Done;
				Task::enable();
			}
			break;
		case TestStateEnum::Done:
			Task::disable();
			break;
		default:
			break;
		}



		/*switch (TestState)
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
		}*/

		return true;
	}

	const bool StartTestMicrosMatchCycles(CycleClock* reference, CycleClock* test, void (*matchTestComplete)(const bool success))
	{
		////TODO: move to task.
		//MatchReference = reference;
		//MatchTest = test;
		//MatchTestComplete = matchTestComplete;

		//if (MatchReference != nullptr
		//	&& MatchTest != nullptr
		//	&& MatchTestComplete != nullptr)
		//{
		//	MatchState = MatchStateEnum::MatchStart;
		//	MatchReference->Start(0);
		//	MatchTest->Start(0);

		//	TestState = TestStateEnum::TestingMatch;

		//	Task::enable();

		//	return true;
		//}

		return false;
	}

private:
	const bool RunTestMicrosMatchCycles()
	{
		uint32_t matchCyclestampReference = 0;
		uint32_t matchCyclestampTest = 0;
		uint32_t elapsed = 0;

		matchCyclestampReference = MatchReference->GetCyclestamp();
		matchCyclestampTest = MatchTest->GetCyclestamp();

		switch (MatchState)
		{
		case MatchStart:
			MatchState = MatchStateEnum::MatchCompare;
			MatchOffset = matchCyclestampTest - matchCyclestampReference;
			MatchStartTimestamp = millis();

			Serial.println(F("Match initial offset: "));
			Serial.println(MatchOffset);

			Task::enable();
			break;
		case MatchCompare:
			MatchError = (matchCyclestampTest + MatchOffset) - matchCyclestampReference;
			if (MatchError > 99)
			{
				MatchState = MatchStateEnum::MatchFail;
				Serial.print(F("Match error too large: "));
				Serial.println(MatchError);
				Serial.print(F("Timestamp Reference:\n\t"));
				Serial.print(matchCyclestampReference);
				Serial.print(F("Timestamp Test:\n\t"));
				Serial.println(matchCyclestampTest);
				Task::enable();
			}
			else
			{
				elapsed = millis() - MatchStartTimestamp;

				if (elapsed > 5000)
				{
					MatchState = MatchStateEnum::MatchSuccess;
					Task::enable();
				}
				else
				{
					Task::delay(1);
				}
			}
			break;
		case MatchSuccess:
			MatchReference->Stop();
			MatchTest->Stop();
			Task::disable();
			MatchTestComplete(true);
			return true;
			break;
		case MatchFail:
			MatchReference->Stop();
			MatchTest->Stop();
			Task::disable();
			MatchTestComplete(false);
			return true;
			break;
		default:
			break;
		}

		return false;
	}
};
#endif

