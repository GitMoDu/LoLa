#define WAIT_FOR_LOGGER

#define SERIAL_BAUD_RATE 250000

// Test sync millis, instead of sync micros.
//#define TEST_MILLIS

#define _TASK_OO_CALLBACKS
#define _TASK_SLEEP_ON_IDLE_RUN

#define DEBUG_LOLA_CLOCK

#include <TaskScheduler.h>
#include <TestIncludes.h>
#include <LoLaDefinitions.h>
#include <LoLaClock\TickTrainedTimerSyncedClock.h>

Scheduler SchedulerBase;

TickTrainedTimerSyncedClock SyncedClock;

// Furioulsy polls the sync micros, to check for non-monoticity, a.k.a. time moves forward only yo.
class TestMonoticityTask : private Task
{
private:
	LoLaSyncedClock* SyncedClock = nullptr;

	uint32_t LastTimestamp = 0;
	uint32_t Timestamp = 0;

public:
	TestMonoticityTask(Scheduler* scheduler)
		: Task(0, TASK_FOREVER, scheduler, false)
	{
	}

	bool OnEnable()
	{
		return SyncedClock != nullptr;
	}

	void Start()
	{
		enable();
	}

	bool Setup(LoLaSyncedClock* syncedClock)
	{
		SyncedClock = syncedClock;

		return SyncedClock != nullptr;
	}

	bool Callback()
	{
		if (SyncedClock->HasTraining())
		{
#ifdef TEST_MILLIS
			Timestamp = SyncedClock->GetSyncMillis() % UINT32_MAX;
#else
			Timestamp = SyncedClock->GetSyncMicros() % UINT32_MAX;
#endif

			if (Timestamp < LastTimestamp)
			{
				Serial.print(F("! Non-monotonic clock detected. Delta: -"));
				Serial.println((LastTimestamp - Timestamp));
			}

			LastTimestamp = Timestamp;
		}
		else
		{
#ifdef TEST_MILLIS
			LastTimestamp = SyncedClock->GetSyncMillis() % UINT32_MAX;
#else
			LastTimestamp = SyncedClock->GetSyncMicros() % UINT32_MAX;
#endif
			Task::delay(1000);
		}
	}
} TestMonoticity(&SchedulerBase);


// Prints out the current sync micros every scheduller millis, for one second.
// Great for low level debug, allows you to check what your response is coming out.
class SecondsTimerTask : private Task
{
private:
	LoLaSyncedClock* SyncedClock = nullptr;

	uint32_t PrintCount = 0;
	uint32_t Timestamp = 0;

public:
	SecondsTimerTask(Scheduler* scheduler)
		: Task(1, TASK_FOREVER, scheduler, false)
	{
	}

	bool OnEnable()
	{
		return SyncedClock != nullptr;
	}

	void Start()
	{
		enable();
	}

	bool Setup(LoLaSyncedClock* syncedClock)
	{
		SyncedClock = syncedClock;

		return SyncedClock != nullptr;
	}

	bool Callback()
	{
		if (SyncedClock->HasTraining())
		{
#ifdef TEST_MILLIS
			Timestamp = SyncedClock->GetSyncMillis() % UINT32_MAX;
#else
			Timestamp = SyncedClock->GetSyncMicros() % UINT32_MAX;
#endif
			Serial.print(millis());
#ifdef TEST_MILLIS
			Serial.print(F(" - SyncMillis: "));
#else
			Serial.print(F(" - SyncMicros: "));
#endif

			Serial.print(Timestamp);
			Serial.println();

			PrintCount++;
			if (PrintCount > 1001)
			{
				disable();
			}
		}
		else
		{
			Task::delay(1000);
		}

		return true;
	}
} SecondsTimer(&SchedulerBase);

void Halt()
{
	Serial.println("[ERROR] Critical Error");
	delay(1000);

	while (1);;
}

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
#ifdef WAIT_FOR_LOGGER
	while (!Serial)
		;
#else
	delay(1000);
#endif
	delay(1000);
	Serial.println(F("LoLa Clock Test Setup"));

	if (!SyncedClock.Setup())
	{
		Serial.println(F("Synced Clock failed Setup."));
		Halt();
	}

	SecondsTimer.Setup(&SyncedClock);
	TestMonoticity.Setup(&SyncedClock);

	if (!StaticTests())
	{
		Serial.println(F("Static tests failed."));
		Halt();
	}

	TestMonoticity.Start();
	SecondsTimer.Start();


	Serial.println();
	Serial.println(F("LoLa Clock Test Start."));
	Serial.println();
}

bool StaticTests()
{
	uint32_t OffsetSeconds = random(INT32_MAX);

	bool Fail = true;
	noInterrupts();
	SyncedClock.SetOffsetSeconds(OffsetSeconds);
	Fail = SyncedClock.GetSyncSeconds() != OffsetSeconds;
	interrupts();

	if (Fail)
	{
		return false;
	}

	return true;
}

void loop()
{
	SchedulerBase.execute();
}