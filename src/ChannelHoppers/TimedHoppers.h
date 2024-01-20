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
	enum class HopperStateEnum
	{
		Disabled,
		StartHop,
		TimedHop
	};

	static constexpr uint32_t MARGIN_MILLIS = 1;

	static constexpr uint32_t HopPeriodMillis()
	{
		return HopPeriodMicros / 1000;
	}

	static constexpr bool PeriodBiggerThanMargin()
	{
		return HopPeriodMillis() > MARGIN_MILLIS;
	}

#if defined(LOLA_UNIT_TESTING)
public:
#else
private:
#endif
	static constexpr uint32_t GetDelayPeriod()
	{
		// Constexpr workaround for conditional.
		return (PeriodBiggerThanMargin() * (HopPeriodMillis() - MARGIN_MILLIS));
	}

private:
	IChannelHop::IHopListener* Listener = nullptr;

	IRollingTimestamp* RollingTimestamp = nullptr;

	uint32_t LastHopIndex = 0;
	uint32_t LastHop = 0;

	uint16_t HopTimestampOffset = 0;

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

	virtual void SetHopTimestampOffset(const uint16_t offset) final
	{
		HopTimestampOffset = offset;
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
		// Set first hop index, so GetTimedHopIndex() is accurate before first hop.
		LastHopIndex = GetHopIndex(RollingTimestamp->GetRollingTimestamp());

		// Run task to start hop.
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
		const uint32_t rollingTimestamp = RollingTimestamp->GetRollingTimestamp();

		switch (HopperState)
		{
		case HopperStateEnum::StartHop:
			LastHopIndex = GetHopIndex(rollingTimestamp);
			LastHop = millis();
			// Fire first notification, to make sure we're starting on the right Rx channel.
			Listener->OnChannelHopTime();
			HopperState = HopperStateEnum::TimedHop;
			Task::enable();
			break;
		case HopperStateEnum::TimedHop:
			if (HopSync(GetHopIndex(rollingTimestamp + HopTimestampOffset)))
			{
#if defined(HOP_TEST_PIN)
				digitalWrite(HOP_TEST_PIN, hopIndex & 0x01);
#endif
				LastHop = millis();
				Listener->OnChannelHopTime();
			}
			break;
		case HopperStateEnum::Disabled:
		default:
			Task::disable();
			return false;
		}

		return true;
	}

private:
	/// <summary>
	/// </summary>
	/// <param name="hopIndex"></param>
	/// <returns>True if it's time to hop.</returns>
	const bool HopSync(const uint32_t hopIndex)
	{
		const uint32_t delta = hopIndex - LastHopIndex;

		if (delta < INT32_MAX)
		{
			LastHopIndex += delta;
			const uint32_t elapsed = millis() - LastHop;

			if ((elapsed >= HopPeriodMillis())
				&& (elapsed <= HopPeriodMillis() + 1))
			{
				// We've just hopped index in the expected time.
				// So, we can sleep for the estimated delay.
				Task::delay(GetDelayPeriod());
			}
			else
			{
				// Out of sync, keep in spin lock.
				Task::enable();
			}

			return true;
		}
		else
		{
			// Detected counter rollback.
#if defined(DEBUG_LOLA)
			Serial.print(F("Counter rollback "));
			Serial.print(LastHopIndex);
			Serial.print(F("->"));
			Serial.println(hopIndex);
#endif
			Task::enable();
		}

		return false;
	}
};
#endif