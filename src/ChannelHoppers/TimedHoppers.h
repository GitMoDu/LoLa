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
	enum class HopperStateEnum : uint8_t
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

	static constexpr bool PeriodBiggerMillisecond()
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
		return (PeriodBiggerThanMargin() * (HopPeriodMillis() - MARGIN_MILLIS));
	}

private:
	IChannelHop::IHopListener* Listener = nullptr;

	IRollingTimestamp* RollingTimestamp = nullptr;

	uint32_t LastTimestamp = 0;
	uint32_t LastHopIndex = 0;
	uint32_t LastHop = 0;

	uint16_t LookForwardOffset = 0;

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
		LookForwardOffset = offset;
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
		Task::enableDelayed(0);
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
			LastTimestamp = rollingTimestamp;
			LastHop = millis();
			HopperState = HopperStateEnum::TimedHop;
			Task::delay(0);
			break;
		case HopperStateEnum::TimedHop:
			//Check if it's time to hop, with forward look compensation.
			if (HopSync(rollingTimestamp + LookForwardOffset))
			{
#if defined(HOP_TEST_PIN)
				digitalWrite(HOP_TEST_PIN, LOW);
				digitalWrite(HOP_TEST_PIN, HIGH);
#endif
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
	/// Synchronized Hop Index update.
	/// Throttles the task sleep according to scheduler overrun/delay.
	/// Skips update if timestamp rollback is detected.
	/// </summary>
	/// <param name="hopIndex"></param>
	/// <returns>True if it's time to hop.</returns>
	const bool HopSync(const uint32_t rollingTimestamp)
	{
		if (rollingTimestamp - LastTimestamp < ((uint32_t)INT32_MAX))
		{
			LastTimestamp = rollingTimestamp;

			const uint32_t now = millis();
			const uint32_t hopIndex = GetHopIndex(rollingTimestamp);

			if (LastHopIndex != hopIndex)
			{
				LastHop = now;
				LastHopIndex = hopIndex;

				const uint32_t elapsed = now - LastHop;

				if (PeriodBiggerMillisecond()
					&& (elapsed >= HopPeriodMillis())
					&& (elapsed <= HopPeriodMillis() + 1))
				{
					// We've just hopped index in the expected time.
					// So, we can sleep for the estimated delay.
					Task::delay(GetDelayPeriod());
				}
				else
				{
					// Out of scheduler sync, keep in spin lock.
					Task::delay(0);
				}

				return true;
			}
		}
		else
		{
#if defined(DEBUG_LOLA)
			Serial.print(F("Timestamp Rollback: "));
			Serial.print(LastTimestamp);
			Serial.print(F("->"));
			Serial.println(rollingTimestamp);
#endif
		}

		Task::delay(0);

		return false;
	}
};
#endif