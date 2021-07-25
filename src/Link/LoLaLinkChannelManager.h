// LoLaLinkChannelManager.h

#ifndef _LOLA_LINK_CHANNEL_MANAGER_h
#define _LOLA_LINK_CHANNEL_MANAGER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <Link\LoLaLinkDefinitions.h>
#include <LoLaClock\LoLaClock.h>
#include <FastCRC.h>

#include <PacketDriver\BaseLoLaDriver.h>
#include <PacketDriver\LoLaPacketDriverSettings.h>

class LoLaLinkChannelManager : private Task, public virtual ISyncedCallbackTarget
{
private:
	BaseLoLaDriver* LoLaDriver = nullptr;
	PacketIOInfo* IOInfo = nullptr;
	LoLaLinkInfo* LinkInfo = nullptr;

	static const uint16_t HopPeriodMillis = LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS;
	static const uint32_t HopPeriodMicros = HopPeriodMillis * IClock::ONE_MILLI_MICROS;

	// Scheduler timer can't be that off, but give it some room to breathe.
	static const uint32_t SleepPeriodMillis = HopPeriodMillis / 2;

	TOTPMillisecondsTokenGenerator* TokenGenerator;

	LoLaPacketDriverSettings* DriverSettings;

	volatile uint8_t ChannelOffset = 0;
	volatile bool HopEnabled = false;

	// SMBUS8 Pseudo-random generator, for channel spreading based on temporal token.
	// If changed, update ProtocolVersionCalculator.
	FastCRC8 FastHasher;

	struct EntropyStruct
	{
		uint8_t array[sizeof(uint32_t)];
		uint32_t uint;
	} Entropy;

public:
	LoLaLinkChannelManager(Scheduler* scheduler,
		BaseLoLaDriver* lolaDriver,
		LoLaPacketDriverSettings* settings,
		TOTPMillisecondsTokenGenerator* tokenGenerator)
		: Task(0, TASK_FOREVER, scheduler, false)
		, ISyncedCallbackTarget()
		, LoLaDriver(lolaDriver)
		, DriverSettings(settings)
		, FastHasher()
		, TokenGenerator(tokenGenerator)
	{
		LoLaDriver->SyncedClock.SetCallbackTarget(this);

#ifdef DEBUG_LINK_FREQUENCY_HOP
		pinMode(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, OUTPUT);
		digitalWrite(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, LOW);
#endif
	}

	// Interrupt callback triggered by the timer.
	virtual void InterruptCallback()
	{
#ifdef DEBUG_LINK_FREQUENCY_HOP
		digitalWrite(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, HIGH);
#endif
		// Wake up packet driver, change the channel ASAP.
		LoLaDriver->InvalidateChannel();

		// Set timed hopper for next hop,
		// but give the packet driver time update the channel.
		Task::enableDelayed(SleepPeriodMillis);
	}

	void StartHopping()
	{
#ifdef LOLA_LINK_USE_CHANNEL_HOP
		HopEnabled = true;
		Task::enable();
#endif
	}

	void StopHopping()
	{
		LoLaDriver->SyncedClock.CancelCallback();
		HopEnabled = false;
		Task::disable();
	}

	const uint8_t NextRandomChannel()
	{
		//TODO: Replace random with proper randomness source.
		ChannelOffset = random(INT32_MAX) % GetChannelRange();

		return DriverSettings->ChannelMin + ChannelOffset;
	}

	const uint8_t NextChannel()
	{
		ChannelOffset = (ChannelOffset + 1) % GetChannelRange();

		return DriverSettings->ChannelMin + ChannelOffset;
	}

	const uint8_t SetChannelDirect(const uint8_t channel)
	{
		ChannelOffset = channel - DriverSettings->ChannelMin;

		return DriverSettings->ChannelMin + ChannelOffset;
	}

	const uint8_t GetChannel()
	{
		if (HopEnabled)
		{
			Entropy.uint = TokenGenerator->GetToken();
			ChannelOffset = FastHasher.smbus(Entropy.array, sizeof(uint32_t)) % GetChannelRange();
		}

		return DriverSettings->ChannelMin + ChannelOffset;
	}

	// Task callback that fires after an interrupt.
	bool Callback()
	{
#ifdef DEBUG_LINK_FREQUENCY_HOP
		digitalWrite(DEBUG_LINK_FREQUENCY_HOP_DEBUG_PIN, LOW);
#endif
		Task::disable();

		if (HopEnabled)
		{
			// Delay micros until next hop.
					// Assumes we are still in the same hop period as the interrupt was fired.
			uint32_t SyncMicros = LoLaDriver->SyncedClock.GetSyncMicros();
			uint32_t NextHopTimestamp = ((SyncMicros / HopPeriodMicros) + 1) * HopPeriodMicros;

			// Requests interrupt callback for the next timed hop.
			LoLaDriver->SyncedClock.StartCallbackAfterMicros(NextHopTimestamp - SyncMicros + 1);
		}
        
        return true;
	}

private:
	const uint8_t GetChannelRange()
	{
		return DriverSettings->ChannelMax - DriverSettings->ChannelMin + 1;
	}
};
#endif