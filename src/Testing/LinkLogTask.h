// LinkLogTask.h

#ifndef _LINK_LOG_TASK_h
#define _LINK_LOG_TASK_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <ILoLaInclude.h>

template<uint32_t PeriodMillis = 1000>
class LinkLogTask : private TS::Task, public virtual ILinkListener
{
private:
	ILoLaLink* Link;
	LoLaLinkExtendedStatus LinkStatus{};

public:
	LinkLogTask(TS::Scheduler* scheduler, ILoLaLink* link)
		: ILinkListener()
		, TS::Task(PeriodMillis, TASK_FOREVER, scheduler, false)
		, Link(link)
	{
	}

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
			TS::Task::enableDelayed(1000);
		}
		else
		{
			Serial.print(millis());
			Serial.println(F("\tLink Lost."));
			TS::Task::disable();
		}
	}

public:
	bool Callback() final
	{
		Link->GetLinkStatus(LinkStatus);
		LinkStatus.OnUpdate();

#if defined(DEBUG_LOLA) || defined(DEBUG_LOLA_LINK)
		LinkStatus.LogLong(Serial);
#endif

		return true;
	}
};
#endif

