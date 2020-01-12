#define WAIT_FOR_LOGGER

#define SERIAL_BAUD_RATE 500000

#define _TASK_OO_CALLBACKS
#define _TASK_SLEEP_ON_IDLE_RUN

#include <TaskScheduler.h>
#include <TestIncludes.h>
#include <LoLaDefinitions.h>
#include <LoLaClock\TrainableTimerTimestampSource.h>

Scheduler SchedulerBase;

TickTrainedTimerSyncedClockWithCallback SyncedClock;

// Furioulsy polls the sync micros, to check for non-monoticity, a.k.a. time moves forward only yo.
class TestMonoticityTask : private Task
{
private:
	LoLaSyncedClock* SyncedClock = nullptr;

	uint32_t LastSyncMicros = 0;
	uint32_t Micros = 0;

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
			Micros = SyncedClock->GetSyncMicros() % UINT32_MAX;

			if (Micros < LastSyncMicros)
			{
				SyncedClock->Debug();
				Serial.print(F("! Non-monotonic clock detected. Delta: -"));
				Serial.println((LastSyncMicros - Micros));
			}

		}
		else
		{
			LastSyncMicros = SyncedClock->GetSyncMicros() % UINT32_MAX;
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
	uint32_t Micros = 0;

public:
	SecondsTimerTask(Scheduler* scheduler)
		: Task(1, TASK_FOREVER, scheduler, false)
	{
	}

	bool OnEnable()
	{
		return SyncedClock != nullptr;
	}

	void StartDelayed(const uint32_t delay)
	{
		enableDelayed(delay);
	}

	void Start()
	{
		PrintCount = 0;
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
			Micros = SyncedClock->GetSyncMicros() % UINT32_MAX;
			Serial.print(millis());
			Serial.print(F(" - SyncMicros: "));
			Serial.println(Micros);

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
	TestMonoticity.Start();
	SecondsTimer.Start();

	Serial.println();
	Serial.println(F("LoLa Clock Test Start."));
	Serial.println();
}

void loop()
{
	SchedulerBase.execute();
}