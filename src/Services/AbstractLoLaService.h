// AbstractLoLaService.h

#ifndef _ABSTRACT_LOLA_SERVICE_h
#define _ABSTRACT_LOLA_SERVICE_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>


#include <Packet\PacketDefinition.h>
#include <PacketDriver\LoLaPacketDriver.h>

// Provides task callback without exposing the underlying task class.
class AbstractLoLaService : private Task, public virtual ILoLaService
{
protected:
	LoLaPacketDriver* LoLaDriver = nullptr;

public:
	AbstractLoLaService(Scheduler* scheduler, const uint32_t period, LoLaPacketDriver* driver)
		: Task(period, TASK_FOREVER, scheduler, false)
		, ILoLaService()
		, LoLaDriver(driver)
	{
	}

public:
	// Returning false denies Ack response, if packet has Ack.
	virtual bool OnPacketReceived(PacketDefinition* definition, const uint32_t timestamp, uint8_t* payload)
	{
		return false;
	}

	virtual void OnAckReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp) {}

	virtual void OnPacketSent(const uint8_t header, const uint8_t id, const uint32_t timestamp) {}

	virtual void OnLinkStatusChanged(const bool linked) {}

	virtual bool Setup()
	{
		if (LoLaDriver != nullptr)
		{
			return true;
		}

		return false;
	}

	// return true if run was "productive - this will disable sleep on the idle run for next pass
	virtual bool Callback()
	{
		return false;
	}

	void Enable()
	{
		Task::enableIfNot();
	}

	void Disable()
	{
		Task::disable();
	}


#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print(' ');
		PrintName(serial);
	}
#endif // DEBUG_LOLA

public:
	virtual bool OnEnable() { return LoLaDriver != nullptr; }
	virtual void OnDisable() {}

protected:
	virtual bool ShouldProcessReceived()
	{
		return LoLaDriver->LinkInfo.HasLink();
	}

#ifdef DEBUG_LOLA
	virtual void PrintName(Stream* serial)
	{
		serial->print(F("Anonymous"));
	}
#endif

protected:
	void SetNextRunDelay(const uint32_t duration)
	{
		if (duration > 0)
		{
			Task::delay(duration);
		}
		else
		{
			Task::delay(1);
		}
	}

	void SetNextRunDefault()
	{
		Task::delay(Task::getInterval());
	}

	void SetNextRunASAP()
	{
		Task::forceNextIteration();
	}
};

#endif

