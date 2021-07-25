#define WAIT_FOR_LOGGER

#define SERIAL_BAUD_RATE 250000


#define _TASK_OO_CALLBACKS
#define _TASK_SLEEP_ON_IDLE_RUN

#define DEBUG_LINK_FREQUENCY_HOP

#include <TaskScheduler.h>
#include <TestIncludes.h>
#include <LoLaDefinitions.h>
#include <Services\Link\LoLaLinkTimedHopper.h>
#include <LoLaClock\TickTrainedTimerSyncedClock.h>


#include <LinkedList.h>

Scheduler SchedulerBase;

TickTrainedTimerSyncedClock SyncedClock;
LoLaLinkTimedHopper TimedHopper(&SchedulerBase);

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
	Serial.println(F("LoLa TimedHopper Test Setup"));

	if (!SyncedClock.Setup())
	{
		Serial.println(F("Synced Clock failed Setup."));
		Halt();
	}

	if (!TimedHopper.Setup(&SyncedClock))
	{
		Serial.println(F("Timed Hopper failed Setup."));
		Halt();
	}

	TimedHopper.Start();

	Serial.println();
	Serial.println(F("LoLa TimedHopper Test Start."));
	Serial.println();
}

void loop()
{
	SchedulerBase.execute();
}