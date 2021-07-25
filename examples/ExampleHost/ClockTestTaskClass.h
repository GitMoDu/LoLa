// ClockTestTaskClass.h

#ifndef _CLOCKTESTTASKCLASS_h
#define _CLOCKTESTTASKCLASS_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <PacketDriver\LoLaAsyncPacketDriver.h>


class ClockTestTaskClass : Task, public virtual ISyncedCallbackTarget
{
private:
	LoLaAsyncPacketDriver* LoLaDriver;

	static const uint32_t Target = 50000;

	volatile uint32_t Requested = 0;
	volatile uint32_t Duration = 0;
	volatile bool HasResult = false;

	bool HasStarted = false;

public:
	ClockTestTaskClass(Scheduler* scheduler, LoLaAsyncPacketDriver* driver)
		: Task(0, TASK_FOREVER, scheduler, true)
		, ISyncedCallbackTarget()
		, LoLaDriver(driver)
	{}

	uint32_t Index = 2;
	uint32_t LastSyncSeconds = 0;

	bool OnEnable()
	{
		LoLaDriver->SyncedClock.SetOffsetSeconds(UINT32_MAX - 9);
		return true;
	}

	void PlotSeconds()
	{
		uint32_t SyncMicros;
		uint32_t Micros;

		SyncMicros = LoLaDriver->SyncedClock.GetSyncMicros();
		Micros = micros();
		uint32_t SyncSeconds = SyncMicros / 1000000L;

		if (LoLaDriver->SyncedClock.HasTraining() && SyncSeconds > LastSyncSeconds)
		{
			LastSyncSeconds = SyncSeconds;
			Serial.print(Index);
			Serial.print('\t');
			Serial.print(SyncMicros);
			Serial.print('\t');
			Serial.println(Micros);
			Index++;
		}
	}

	void PlotConsistency()
	{
		uint32_t Value;
		uint32_t CaptureDuration;
		CaptureDuration = micros();
		Value = LoLaDriver->SyncedClock.GetSyncSeconds();
		CaptureDuration = micros() - CaptureDuration;

		Serial.print("Seconds: ");
		Serial.print(Value);
		Serial.print(" took ");
		Serial.print(CaptureDuration * 1000);
		Serial.print(" ns ");
		Serial.println();

		CaptureDuration = micros();
		Value = LoLaDriver->SyncedClock.GetSyncMillis();
		CaptureDuration = micros() - CaptureDuration;

		Serial.print("Millis: ");
		Serial.print(Value);
		Serial.print(" took ");
		Serial.print(CaptureDuration * 1000);
		Serial.print(" ns ");
		Serial.println();

		CaptureDuration = micros();
		Value = LoLaDriver->SyncedClock.GetSyncMicros();
		CaptureDuration = micros() - CaptureDuration;

		Serial.print("Micros: ");
		Serial.print(Value);
		Serial.print(" took ");
		Serial.print(CaptureDuration * 1000);
		Serial.print(" ns ");
		Serial.println();

		Serial.println();
		Task::delay(1000);
	}

	void TestCallback()
	{
		if (HasResult)
		{
			HasResult = false;
			Serial.print(F("Last Duration: "));
			Serial.println(Duration);
			Task::delay(1000);
		}
		else if (!HasStarted && LoLaDriver->SyncedClock.HasTraining())
		{
			HasStarted = true;
			LoLaDriver->SyncedClock.SetCallbackTarget(this);
			Serial.println(F("Clock Ready, SetCallbackTarget"));
			Requested = 0;
			Task::delay(1000);
		}
		else if (HasStarted)
		{
			HasResult = false;
			Task::delay(0);
			Task::disable();
			Requested = LoLaDriver->SyncedClock.GetSyncMicros();
			LoLaDriver->SyncedClock.StartCallbackAfterMicros(Target);
		}
		else
		{
			Task::delay(0);
		}
	}

	void PlotTraining()
	{
		const uint32_t StepDurationNanos = LoLaDriver->SyncedClock.GetStepDurationNanos();
		Serial.print("Timer trained: ");
		Serial.print(StepDurationNanos);
		Serial.print(" ns per Step|\t");
		Serial.print("Steps per Second: ");
		Serial.println(IClock::ONE_SECOND_NANOS / StepDurationNanos);
		Serial.println();
		Task::delay(1000);
	}


	bool Callback()
	{
		//PlotTraining();
		//PlotSeconds();
		PlotConsistency();
		//TestCallback();

		return true;
	}

	virtual void InterruptCallback()
	{
		Duration = LoLaDriver->SyncedClock.GetSyncMicros() - Requested;
		HasResult = true;
		Task::enable();
	}
};

#endif

