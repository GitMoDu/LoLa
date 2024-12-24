// TimedHoppers.h

#ifndef _TIMED_HOPPERS_h
#define _TIMED_HOPPERS_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

template<const uint32_t HopPeriodMicros,
	const uint16_t ForwardLookMicros = 10>
class TimedChannelHopper final : private TS::Task, public virtual IChannelHop
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

	LinkClock* SyncClock = nullptr;

	uint32_t LastTimestamp = 0;
	uint32_t LastHopIndex = 0;
	uint32_t LastHop = 0;

	HopperStateEnum HopperState = HopperStateEnum::Disabled;

	uint8_t FixedChannel = 0;

public:
	TimedChannelHopper(TS::Scheduler& scheduler)
		: IChannelHop()
		, TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{}

	const bool Setup(IChannelHop::IHopListener* listener, LinkClock* linkClock) final
	{
		Listener = listener;
		SyncClock = linkClock;

		return SyncClock != nullptr && Listener != nullptr && HopPeriodMicros > 1;
	}

	// General Channel Interfaces //
	const uint8_t GetChannel() final
	{
		return FixedChannel;
	}

	virtual void SetChannel(const uint8_t channel) final
	{
		FixedChannel = channel;
	}
	////

	// Hopper Interfaces //
	const uint32_t GetHopPeriod() final
	{
		return HopPeriodMicros;
	}

	const uint32_t GetHopIndex(const uint32_t timestamp) final
	{
		return timestamp / HopPeriodMicros;
	}

	const uint32_t GetTimedHopIndex() final
	{
		return LastHopIndex;
	}

	void OnLinkStarted() final
	{
		// Set starting hop.
		LastTimestamp = SyncClock->GetRollingMicros();
		LastHopIndex = GetHopIndex(LastTimestamp);

		// Run task to synchronize first hop.
		HopperState = HopperStateEnum::StartHop;
		TS::Task::enableDelayed(0); 
		Listener->OnChannelHopTime();
	}

	void OnLinkStopped() final
	{
		HopperState = HopperStateEnum::Disabled;
		TS::Task::disable();
	}
	////

	bool Callback() final
	{
		const uint32_t rollingTimestamp = SyncClock->GetRollingMicros();

		switch (HopperState)
		{
		case HopperStateEnum::StartHop:
			// Synchronize first hop.
			if ((rollingTimestamp - LastTimestamp < ((uint32_t)INT32_MAX))
				&& (GetHopIndex(rollingTimestamp) != LastHopIndex))
			{
				LastHopIndex = GetHopIndex(rollingTimestamp);
				LastHop = millis();
				HopperState = HopperStateEnum::TimedHop;
				Listener->OnChannelHopTime();
			}
			TS::Task::delay(0);
			break;
		case HopperStateEnum::TimedHop:
			// Check if it's time to hop, with forward look compensation.
			if (HopSync(rollingTimestamp + ForwardLookMicros))
			{
				Listener->OnChannelHopTime();
			}
			break;
		case HopperStateEnum::Disabled:
		default:
			TS::Task::disable();
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
					TS::Task::delay(GetDelayPeriod());
				}
				else
				{
					// Out of scheduler sync, keep in spin lock.
					TS::Task::delay(0);
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
			// Out of clock sync, keep in spin lock.
			TS::Task::delay(0);
			return true;
		}

		TS::Task::delay(0);

		return false;
	}
};
#endif