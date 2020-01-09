// LoLaLinkTimedHopper.h

#ifndef _LOLA_TIMED_HOPPER_h
#define _LOLA_TIMED_HOPPER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <LoLaClock\IClock.h>
#include <Services\Link\LoLaLinkDefinitions.h>

#include <LoLaCrypto\CryptoTokenGenerator.h>
#include <Services\Link\LoLaLinkChannelManager.h>


class LoLaLinkTimedHopper : private Task,
	public virtual ISyncedCallbackTarget
{
private:
	const uint32_t OverheadMarginMicros = 10;

	const uint32_t MinPeriodMillis = 5;
	const uint32_t MaxPeriodMillis = 1000;

	static const uint32_t ONE_MILLI_MICROS = 1000;

	static const uint8_t HopPeriodMillis = LOLA_LINK_SERVICE_LINKED_TOKEN_HOP_PERIOD_MILLIS;
	static const uint32_t HopPeriodMicros = HopPeriodMillis * ONE_MILLI_MICROS;

	ISyncedClock* SyncedClock = nullptr;
	ILoLaDriver* LoLaDriver = nullptr;

	ISyncedCallbackSource* CallbackSource = nullptr;

public:
	LoLaLinkTimedHopper(Scheduler* scheduler, ISyncedClock* syncedClock)
		: Task(0, TASK_FOREVER, scheduler, false)
		, ISyncedCallbackTarget()
	{
		SyncedClock = syncedClock;

		if (SyncedClock != nullptr)
		{
			CallbackSource = SyncedClock->GetSyncedCallbackSource();
		}
	}

	virtual void InterruptCallback()
	{
#ifdef DEBUG_LINK_FREQUENCY_HOP
		digitalWrite(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, HIGH);
#endif

		// Wake up packet driver, channel has changed.
		LoLaDriver->OnChannelUpdated();

		// Enable to set up next interrupt.
		// but give the packet driver time update channel.
		enableDelayed(MinPeriodMillis);
	}

	bool Callback()
	{
		disable();

		// Delay = Hop period 
		// - some margin for the radio driver
		// - elapsed micros since last hop 
		CallbackSource->StartCallbackAfterMicros(HopPeriodMicros
			- OverheadMarginMicros
			- (SyncedClock->GetSyncMicros() % HopPeriodMicros));

#ifdef DEBUG_LINK_FREQUENCY_HOP
		digitalWrite(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, LOW);
#endif
	}

	bool Setup()
	{
		if (SyncedClock != nullptr &&
			CallbackSource != nullptr &&
			HopPeriodMillis <= MaxPeriodMillis &&
			HopPeriodMillis > MinPeriodMillis)
		{
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
		serial->print(F("LinkTimedHopper (Cryp"));
		serial->print('|');
#ifdef LOLA_LINK_USE_FREQUENCY_HOP
		serial->print(F("Freq"));
#endif 
		serial->print(')');
	}
#endif // DEBUG_LOLA
};
#endif