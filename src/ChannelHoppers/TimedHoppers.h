// TimedHoppers.h

#ifndef _TIMED_HOPPERS_h
#define _TIMED_HOPPERS_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "..\Link\IChannelHop.h"
#include "..\Clock\SynchronizedClock.h"
#include "..\Clock\Timestamp.h"

template<const uint32_t HopPeriodMicros>
class TimedChannelHopper final : private Task, public virtual IChannelHop
{
private:
	enum HopperStateEnum
	{
		Disabled,
		StartHop,
		SpinLock
	};

	static constexpr uint32_t MARGIN_MILLIS = 1;
	static constexpr uint32_t CLOCK_MARGIN_PERCENTAGE = 2;

	static constexpr uint32_t HopPeriodMillis()
	{
		return HopPeriodMicros / 1000;
	}

	static constexpr bool PeriodOverZero()
	{
		return HopPeriodMillis() > 0;
	}

#ifdef LOLA_UNIT_TESTING
public:
#endif
	static constexpr uint32_t GetDelayPeriod()
	{
		// Constexpr workaround for conditional.
		return (PeriodOverZero() * (HopPeriodMillis() - 1))
			+ (!PeriodOverZero() * HopPeriodMillis());
	}

private:
	IChannelHop::IHopListener* Listener = nullptr;

	SynchronizedClock* SyncClock = nullptr;

	Timestamp CheckTimestamp{};

	uint32_t LastHopIndex = 0;
	uint32_t HopIndex = 0;

	HopperStateEnum HopperState = HopperStateEnum::Disabled;

	uint8_t FixedChannel = 0;

public:
	TimedChannelHopper(Scheduler& scheduler)
		: IChannelHop()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
#if defined(HOP_TEST_PIN)
		digitalWrite(HOP_TEST_PIN, LOW);
		pinMode(HOP_TEST_PIN, OUTPUT);
#endif
	}

	virtual const bool Setup(IChannelHop::IHopListener* listener, SynchronizedClock* syncClock) final
	{
		Listener = listener;
		SyncClock = syncClock;

		return SyncClock != nullptr && Listener != nullptr && HopPeriodMicros > 1;
	}

	// General Channel Interfaces //
	virtual const uint8_t GetBroadcastChannel() final
	{
		return FixedChannel;
	}

	virtual const uint8_t GetFixedChannel() final
	{
		return FixedChannel;
	}

	virtual void SetFixedChannel(const uint8_t channel) final
	{
		FixedChannel = channel;
	}
	////

	// Hopper Interfaces //
	virtual const uint32_t GetHopPeriod() final
	{
		return HopPeriodMicros;
	}

	virtual const uint32_t GetHopIndex(const uint32_t timestamp) final
	{
		return timestamp / HopPeriodMicros;
	}

	virtual const uint32_t GetTimedHopIndex() final
	{
		return HopIndex;
	}

	virtual void OnLinkStarted() final
	{
		// Set first hop index.
		HopperState = HopperStateEnum::StartHop;
		Task::enable();
	}

	virtual void OnLinkStopped() final
	{
		HopperState = HopperStateEnum::Disabled;
		Task::disable();
	}
	////

	virtual bool Callback() final
	{
		SyncClock->GetTimestamp(CheckTimestamp);
		HopIndex = GetHopIndex(CheckTimestamp.GetRollingMicros());

		switch (HopperState)
		{
		case HopperStateEnum::StartHop:
			// Fire first notification, to make sure we're starting on the right Rx channel.
			LastHopIndex = HopIndex;
			Listener->OnChannelHopTime();
			HopperState = HopperStateEnum::SpinLock;
			Task::enable();
			break;
		case HopperStateEnum::SpinLock:
			if (HopIndex != LastHopIndex)
			{
				LastHopIndex = HopIndex;
#if defined(HOP_TEST_PIN)
				digitalWrite(HOP_TEST_PIN, HIGH);
				digitalWrite(HOP_TEST_PIN, LOW);
#endif
				Listener->OnChannelHopTime();

				Task::delay(GetDelayPeriod());

				// We've just hopped index, we can sleep for the estimated delay with margin.
				return false;
			}
			else
			{
				Task::enable();
			}
			break;
		case HopperStateEnum::Disabled:
		default:
			Task::disable();
			return false;
		}

		return true;
	}
};
#endif