// ILoLaService.h

#ifndef _ILOLA_SERVICE_h
#define _ILOLA_SERVICE_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <LoLaDefinitions.h>
#include <ILoLaDriver.h>
#include <IPacketListener.h>

class ILoLaService : Task, public virtual IPacketListener
{
protected:
	ILoLaDriver* LoLaDriver = nullptr;

public:
	ILoLaService(Scheduler* scheduler, const uint32_t period, ILoLaDriver* driver)
		: Task(period, TASK_FOREVER, scheduler, false)
		, IPacketListener()
	{
		LoLaDriver = driver;
	}

public:
	///IPacketService Calls
	//Returning false denies Ack response, if packet has Ack.
	virtual bool OnPacketReceived(PacketDefinition* definition, const uint8_t id, const uint32_t timestamp, uint8_t* payload)
	{
		return false;
	}

	virtual void OnAckReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp) {}

	virtual void OnPacketSent(const uint8_t header, const uint8_t id, const uint32_t timestamp) {}

	virtual void OnPacketTransmited(const uint32_t timestamp) {}

	virtual void OnLinkStatusChanged(const bool linked) {}

	virtual bool Setup()
	{
		if (LoLaDriver != nullptr)
		{
			return true;
		}

		return false;
	}
	//

	// return true if run was "productive - this will disable sleep on the idle run for next pass
	virtual bool Callback()
	{
		return false;
	}

	void Enable()
	{
		enableIfNot();
	}

	void Disable()
	{
		disable();
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
		return LoLaDriver->HasLink();
	}

#ifdef DEBUG_LOLA
	virtual void PrintName(Stream* serial)
	{
		serial->print(F("Anonymous."));
	}
#endif

protected:
	void SetNextRunDelay(const uint32_t duration)
	{
		Task::delay(max(1, duration));
	}

	void SetNextRunDefault()
	{
		Task::delay(0);
	}

	void SetNextRunASAP()
	{
		forceNextIteration();
	}
};
#endif