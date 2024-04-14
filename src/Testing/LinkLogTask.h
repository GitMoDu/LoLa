// LinkLogTask.h

#ifndef _LINK_LOG_TASK_h
#define _LINK_LOG_TASK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaInclude.h>

class LinkLogTask : private Task, public virtual ILinkListener
{
private:
	static constexpr uint32_t PeriodMillis = 1000;

private:
	ILoLaLink* Link;
	LoLaLinkExtendedStatus LinkStatus{};

public:
	LinkLogTask(Scheduler* scheduler, ILoLaLink* link)
		: ILinkListener()
		, Task(PeriodMillis, TASK_FOREVER, scheduler, false)
		, Link(link)
	{}

	const bool Setup()
	{
		if (Link != nullptr)
		{
			return Link->RegisterLinkListener(this);
		}
		else
		{
			return false;
		}
	}

public:
	virtual void OnLinkStateUpdated(const bool hasLink) final
	{
		if (hasLink)
		{
			Link->GetLinkStatus(LinkStatus);
			LinkStatus.OnStart();
			Serial.print(millis());
			Serial.println(F("\tLink Acquired."));
			Task::enableDelayed(1000);
		}
		else
		{
			Serial.print(millis());
			Serial.println(F("\tLink Lost."));
			Task::disable();
		}
	}

public:
	bool Callback() final
	{
		Link->GetLinkStatus(LinkStatus);
		LinkStatus.OnUpdate();

		LinkStatus.LogLong(Serial);

		return true;
	}
};
#endif

