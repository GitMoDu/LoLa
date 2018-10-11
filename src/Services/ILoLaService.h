// ILoLaService.h

#ifndef _ILOLA_SERVICE_h
#define _ILOLA_SERVICE_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>
#include <ILoLa.h>


#define LOLA_SERVICE_DEFAULT_PERIOD_MILLIS 500
#define LOLA_SERVICE_HOUR_PERIOD_MILLIS 3600000
#define LOLA_SERVICE_LONG_SLEEP_PERIOD_MILLIS 30000

class ILoLaService : Task
{
private:
	enum ServiceStateBasic : uint8_t
	{
		Loading,
		Failed,
		Active
	} ServiceState = Failed;

	ILoLa* LoLa;
	uint16_t DefaultPeriod = 0;

private:
	void InitializeState(ILoLa* loLa)
	{
		LoLa = loLa;
		if (LoLa != nullptr && LoLa->GetPacketMap() != nullptr)
		{
			ServiceState = Loading;
		}
		else
		{
			ServiceState = Failed;
		}
	}

public:
	ILoLaService(Scheduler* scheduler, const uint16_t defaultPeriod, ILoLa* loLa)
		: Task(0, TASK_FOREVER, scheduler, false)
	{
		DefaultPeriod = max(1, defaultPeriod);
		InitializeState(loLa);
	}

	ILoLaService(Scheduler* scheduler, ILoLa* loLa)
		: Task(0, TASK_FOREVER, scheduler, false)
	{
		DefaultPeriod = LOLA_SERVICE_DEFAULT_PERIOD_MILLIS;
		InitializeState(loLa);
	}

	// return true if run was "productive - this will disable sleep on the idle run for next pass
	virtual bool Callback()
	{
		return false;
	}

	ILoLa * GetLoLa()
	{
		return LoLa;
	}

	virtual bool OnEnable()
	{
		return true;
	}

	virtual void OnDisable()
	{
	}

	void Enable()
	{
		enableIfNot();
	}

	void Disable()
	{
		disable();
	}

	bool Init()
	{
		if (OnAddPacketMap(LoLa->GetPacketMap()) && Setup())
		{
			return true;
		}
		else
		{
			ServiceState = Failed;
			return false;
		}
	}

	virtual bool OnSetup() { return true; }

	bool IsSetupOk() { return ServiceState == Active; }

	bool Setup()
	{
		if (ServiceState != Failed)
		{
			ServiceState = Loading;
			if (((ILoLa*)LoLa) != nullptr &&
				GetPacketMap() != nullptr &&
				OnSetup())
			{
				ServiceState = Active;

				return true;
			}
			else
			{
				ServiceState = Failed;
			}
		}

		return false;
	}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print(' ');
		PrintName(serial);
	}
#endif // DEBUG_LOLA

public:
	bool ReceivedPacket(ILoLaPacket* incomingPacket, const uint8_t header) 
	{
		if (ShouldProcessReceived()) {
			return ProcessPacket(incomingPacket, header);
		}
		return false; 
	}

	bool ReceivedAck(const uint8_t header, const uint8_t id) 
	{
		if (ShouldProcessReceived()) {
			return ProcessAck(header, id);
		}
		return false;
	}

public:
	virtual bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header) { return false; }
	virtual bool ProcessAck(const uint8_t header, const uint8_t id) { return false; }
	virtual void OnLinkEstablished() {}
	virtual void OnLinkLost() {}

protected:
	virtual bool ShouldProcessReceived() 
	{
		return IsSetupOk() && LoLa->IsLinkActive();
	}

	virtual bool OnAddPacketMap(LoLaPacketMap* packetMap) { return true; }
#ifdef DEBUG_LOLA
	virtual void PrintName(Stream* serial)
	{
		serial->print(F("Anonymous."));
	}
#endif

protected:
	void SetNextRunDefault()
	{
		Task::delay(DefaultPeriod);
	}

	void SetNextRunLong()
	{
		Task::delay(LOLA_SERVICE_HOUR_PERIOD_MILLIS);
	}

	void SetNextRunDelay(const uint32_t duration)
	{
		Task::delay(duration);
	}

	void SetNextRunASAP()
	{
		forceNextIteration();
	}

	LoLaPacketMap * GetPacketMap()
	{
		return LoLa->GetPacketMap();
	}

	bool AllowedSend(const bool overridePermission = false)
	{
		return LoLa->AllowedSend(overridePermission);
	}

	bool SendPacket(ILoLaPacket* outgoingPacket)
	{
		return LoLa->SendPacket(outgoingPacket);
	}

	uint32_t Millis()
	{
		return LoLa->GetMillis();
	}

	uint32_t MillisSync()
	{
		return LoLa->GetMillis();
	}

	uint32_t Micros()
	{
		return LoLa->GetTimeStamp();
	}
};
#endif