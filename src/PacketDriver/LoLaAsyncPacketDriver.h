// LoLaAsyncPacketDriver.h

#ifndef _LOLA_ASYNC_PACKET_DRIVER_h
#define _LOLA_ASYNC_PACKET_DRIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <PacketDriver\LoLaPacketDriver.h>
#include <PacketDriver\AsyncCheckerTarget.h>


class AsyncCheckerTarget
{
public:
	virtual bool CheckPending() { return false; }
};

class LoLaAsyncPacketDriver : public LoLaPacketDriver, public virtual AsyncCheckerTarget
{
protected:
	class PendingActionsClass
	{
	public:
		volatile uint8_t ActionPending = 0;
		volatile bool RX = false;
		volatile bool Power = false;
		volatile bool Channel = false;

	public:
		void Clear()
		{
			RX = false;
			Power = false;
			Channel = false;
		}

		void ClearChannel()
		{
			//Serial.println(F("Async clear Channel"));
			Channel = false;
		}

		void ClearPower()
		{
			//Serial.println(F("Async clear Power"));
			Power = false;
		}

		void ClearRX()
		{
			//Serial.println(F("Async clear Receive"));
			RX = false;
		}

		void ClearDriverAction()
		{
			//Serial.println(F("Async clear DriverAction"));
			ActionPending = 0;
		}

		const bool HasAny()
		{
			return ActionPending > 0 || RX || Power || Channel;
		}
	};


	class AsyncCheckerTask : Task
	{
	private:
		AsyncCheckerTarget* Checker = nullptr;

	public:
		AsyncCheckerTask(Scheduler* scheduler,
			AsyncCheckerTarget* checker)
			: Task(0, TASK_FOREVER, scheduler, false)
			, Checker(checker)
		{}

		bool Callback()
		{
			if (Checker->CheckPending())
			{
				Task::disable();
				return true;
			}
			else
			{
				Task::forceNextIteration();
				return false;
			}
		}

		void Wake()
		{
			Task::enable();
			Task::forceNextIteration();
		}
	};

	AsyncCheckerTask AsyncChecker;
	PendingActionsClass PendingActions;

protected:
	virtual void DriverCallback(const uint8_t action) {}

public:
	LoLaAsyncPacketDriver(Scheduler* scheduler, LoLaPacketDriverSettings* settings)
		: LoLaPacketDriver(scheduler, settings)
		, AsyncChecker(scheduler, this)
		, AsyncCheckerTarget()
	{
	}

	virtual bool Setup()
	{
		return LoLaPacketDriver::Setup();
	}

	void RequestDriverAction(const uint8_t action)
	{
		if (PendingActions.ActionPending == 0) 
		{
			PendingActions.ActionPending = action;
			AsyncChecker.Wake();
		}
	}

	virtual void InvalidateChannel()
	{
		PendingActions.Channel = true;
		AsyncChecker.Wake();
	}

	virtual void InvalidatedTransmitPower()
	{
		PendingActions.Power = true;
		AsyncChecker.Wake();
	}

	// Returns true when there is no more work to be done.
	virtual bool CheckPending()
	{
		// Driver pending actions have priority.
		if (PendingActions.ActionPending)
		{
			const uint8_t action = PendingActions.ActionPending;
			PendingActions.ClearDriverAction();
			DriverCallback(action);
		}
		else if (PendingActions.RX)
		{
			if (SetRadioReceive())
			{
				PendingActions.ClearRX();
			}
		}
		else if (PendingActions.Channel)
		{
			if (SetRadioChannel())
			{
				PendingActions.ClearChannel();
			}
		}
		else if (PendingActions.Power)
		{
			if (SetRadioPower())
			{
				PendingActions.ClearPower();
			}
		}
		else
		{
			// No more work to be done.
			return true;
		}

		return false;
	}
};
#endif