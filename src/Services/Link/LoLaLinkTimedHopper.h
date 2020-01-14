// LoLaLinkTimedHopper.h

#ifndef _LOLA_TIMED_HOPPER_h
#define _LOLA_TIMED_HOPPER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <LoLaClock\IClock.h>
#include <ILoLaDriver.h>


class LoLaLinkTimedHopper : private Task,
	public virtual ISyncedCallbackTarget
{
private:
	static const uint32_t ONE_MILLI_MICROS = 1000;

	static const uint16_t HopPeriodMillis = LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS;
	static const uint32_t HopPeriodMicros = HopPeriodMillis * ONE_MILLI_MICROS;

	const uint32_t MinPeriodMillis = 1;
	const uint32_t MaxPeriodMillis = 950;

	// Scheduler timer can't be that off, but give it some room to breathe.
	const uint32_t SleepPeriodMillis = (HopPeriodMillis * 8) / 10;

	ISyncedClock* SyncedClock = nullptr;
	ILoLaDriver* LoLaDriver = nullptr;

	ISyncedCallbackSource* CallbackSource = nullptr;

public:
	LoLaLinkTimedHopper(Scheduler* scheduler)
		: Task(0, TASK_FOREVER, scheduler, false)
		, ISyncedCallbackTarget()
	{
	}

	virtual void InterruptCallback()
	{
#ifdef DEBUG_LINK_FREQUENCY_HOP
		digitalWrite(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, HIGH);
#else
		// Wake up packet driver, channel has changed.
		LoLaDriver->OnChannelUpdated();
#endif

		// Enable to set up next interrupt.
		// but give the packet driver time update channel.
		enableDelayed(SleepPeriodMillis);
	}

	void Start()
	{
#ifdef DEBUG_LINK_FREQUENCY_HOP
		enableDelayed(5000);
#else
		enable();
#endif
	}

	void Stop()
	{
		CallbackSource->CancelCallback();
		disable();
	}

	bool Callback()
	{
#ifdef DEBUG_LINK_FREQUENCY_HOP
		digitalWrite(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, LOW);
#endif
		disable();

		// Delay micros until next hop.
		// Assumes we are still in the same hop period as the interrupt was fired.
		uint64_t NextHopTimestamp = ((SyncedClock->GetSyncMicros() / HopPeriodMicros) + 1) * HopPeriodMicros;

		CallbackSource->StartCallbackAfterMicros((NextHopTimestamp - SyncedClock->GetSyncMicros()));
	}

	bool Setup(ISyncedClock* syncedClock)
	{
		SyncedClock = syncedClock;

		if (SyncedClock != nullptr &&
			HopPeriodMillis <= MaxPeriodMillis &&
			HopPeriodMillis >= MinPeriodMillis)
		{
			CallbackSource = SyncedClock;
			CallbackSource->SetCallbackTarget(this);

#ifdef DEBUG_LINK_FREQUENCY_HOP
			pinMode(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, OUTPUT);
			digitalWrite(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, LOW);
#endif

			return true;
		}

		return false;
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("LinkTimedHopper"));
		serial->print('|');
#ifdef LOLA_LINK_USE_FREQUENCY_HOP
		serial->print(F("Freq"));
#endif 
		serial->print(')');
	}
#endif // DEBUG_LOLA
};
#endif