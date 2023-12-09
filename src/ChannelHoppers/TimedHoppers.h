// TimedHoppers.h

#ifndef _TIMED_HOPPERS_h
#define _TIMED_HOPPERS_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <IChannelHop.h>


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

	static constexpr uint32_t HopPeriodMillis()
	{
		return HopPeriodMicros / 1000;
	}

	static constexpr bool PeriodOverZero()
	{
		return HopPeriodMillis() > 0;
	}

#if defined(LOLA_UNIT_TESTING)
public:
#else
private:
#endif
	static constexpr uint32_t GetDelayPeriod()
	{
		// Constexpr workaround for conditional.
		return (PeriodOverZero() * (HopPeriodMillis() - MARGIN_MILLIS))
			+ (!PeriodOverZero() * HopPeriodMillis());
	}

private:
	IChannelHop::IHopListener* Listener = nullptr;

	IRollingTimestamp* RollingTimestamp = nullptr;

	uint32_t LastHopIndex = 0;

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

	virtual const bool Setup(IChannelHop::IHopListener* listener, IRollingTimestamp* rollingTimestamp) final
	{
		Listener = listener;
		RollingTimestamp = rollingTimestamp;

		return RollingTimestamp != nullptr && Listener != nullptr && HopPeriodMicros > 1;
	}

	// General Channel Interfaces //
	virtual const uint8_t GetChannel() final
	{
		return FixedChannel;
	}

	virtual void SetChannel(const uint8_t channel) final
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
		return LastHopIndex;
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
		const uint32_t hopIndex = GetHopIndex(RollingTimestamp->GetRollingTimestamp());

		switch (HopperState)
		{
		case HopperStateEnum::StartHop:
			// Fire first notification, to make sure we're starting on the right Rx channel.
			LastHopIndex = hopIndex;
			Listener->OnChannelHopTime();
			HopperState = HopperStateEnum::SpinLock;
			Task::enable();
			break;
		case HopperStateEnum::SpinLock:
			if (hopIndex != LastHopIndex)
			{
				LastHopIndex = hopIndex;
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