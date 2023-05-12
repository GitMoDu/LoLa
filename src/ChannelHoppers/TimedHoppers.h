// TimedHoppers.h

#ifndef _TIMED_HOPPERS_h
#define _TIMED_HOPPERS_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "..\Link\IChannelHop.h"
#include "..\Clock\SynchronizedClock.h"
#include "..\Clock\Timestamp.h"

template<const uint32_t HopPeriodMicros,
	const uint8_t PinTestHop = 0,
	const uint8_t PinTestHop2 = 0>
class TimedChannelHopper final : private Task, public virtual IChannelHop
{
private:
	enum HopperStateEnum
	{
		Disabled,
		StartHop,
		SpinLock,
	};

	static constexpr uint32_t MARGIN_MILLIS = 1;
	static constexpr uint32_t CLOCK_MARGIN_PERCENTAGE = 5;

	static constexpr uint32_t HopPeriodMillis()
	{
		return HopPeriodMicros / 1000;
	}

#ifdef LOLA_UNIT_TESTING
public:
#endif
	static constexpr uint32_t GetDelayPeriod()
	{
		return ((HopPeriodMillis() * (100 - CLOCK_MARGIN_PERCENTAGE)) / 100);
	}

private:
	IChannelHop::IHopListener* Listener = nullptr;

	SynchronizedClock* SyncClock = nullptr;

	Timestamp CheckTimestamp{};

	uint32_t LastHopIndex = 0;
	uint32_t HopIndex = 0;


	uint32_t ForwardLookMicros = 0;

	HopperStateEnum HopperState = HopperStateEnum::Disabled;

	uint8_t FixedChannel = 0;

public:
	TimedChannelHopper(Scheduler& scheduler)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, IChannelHop()
	{
		if (PinTestHop > 0)
		{
			digitalWrite(PinTestHop, LOW);
			pinMode(PinTestHop, OUTPUT);
		}	
		
		if (PinTestHop2 > 0)
		{
			digitalWrite(PinTestHop2, LOW);
			pinMode(PinTestHop2, OUTPUT);
		}
	}

	virtual const bool Setup(IChannelHop::IHopListener* listener, SynchronizedClock* syncClock, const uint32_t forwardLookMicros) final
	{
		ForwardLookMicros = forwardLookMicros;
		Listener = listener;
		SyncClock = syncClock;

		return SyncClock != nullptr && Listener != nullptr && HopPeriodMicros > 1;
	}

#pragma region General Channel Interfaces
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
#pragma endregion

#pragma region Hopper Interfaces
	virtual const bool IsHopper() final
	{
		return true;
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
#pragma endregion



	virtual bool Callback() final
	{
		SyncClock->GetTimestamp(CheckTimestamp);
		CheckTimestamp.ShiftSubSeconds(ForwardLookMicros);

		if (PinTestHop2 > 0)
		{
			digitalWrite(PinTestHop2, HIGH);
		}
		if (PinTestHop2 > 0)
		{
			digitalWrite(PinTestHop2, LOW);
		}


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
				if (PinTestHop > 0)
				{
					digitalWrite(PinTestHop, HIGH);
				}
				if (PinTestHop > 0)
				{
					digitalWrite(PinTestHop, LOW);
				}
				Listener->OnChannelHopTime();

				// We've just hopped index, we can sleep for the estimated delay with margin.
				Task::delay(GetDelayPeriod());
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