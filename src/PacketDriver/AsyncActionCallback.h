// AsyncActionCallback.h

#ifndef _ASYNCACTIONCALLBACK_h
#define _ASYNCACTIONCALLBACK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <Callback.h>
#include <RingBufCPP.h>


#define ASYNC_ACTION_CALLBACK_EASE_PERIOD_MILLIS	(uint32_t)(1)

template <const uint8_t QueueSize, typename ActionType>
class TemplateAsyncActionCallback : Task
{
private:
	Signal<ActionType> ActionEvent;
	ActionType Grunt;
	RingBufCPP<ActionType, QueueSize> EventQueue;

public:
	TemplateAsyncActionCallback(Scheduler* scheduler)
		: Task(0, TASK_FOREVER, scheduler, false)
	{
	}

	void AttachActionCallback(const Slot<ActionType>& slot)
	{
		ActionEvent.attach(slot);
	}

	void AppendToQueue(ActionType action, const bool easeNextEvent)
	{
		EventQueue.addForce(action);
		enable();
		if (easeNextEvent && EventQueue.numElements() == 1)
		{
			Task::delay(ASYNC_ACTION_CALLBACK_EASE_PERIOD_MILLIS);
		}
	}

	bool OnEnable()
	{
		forceNextIteration();
		return true;
	}

	void OnDisable()
	{
	}

	bool Callback()
	{
		if (!EventQueue.pull(Grunt))
		{
			disable();
			return false;
		}

		ActionEvent.fire(Grunt);
		forceNextIteration();

		return true;
	}
};
#endif