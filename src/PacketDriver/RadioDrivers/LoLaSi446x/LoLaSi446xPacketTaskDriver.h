// LoLaSi446xPacketTaskDriver.h

#ifndef _LOLASI446XPACKETTASKDRIVER_h
#define _LOLASI446XPACKETTASKDRIVER_h


#include <PacketDriver\RadioDrivers\LoLaSi446x\LoLaSi446xPacketDriver.h>
#include <PacketDriver\RadioDrivers\LoLaSi446x\IRadioTask.h>

class LoLaSi446xPacketTaskDriver : public LoLaSi446xPacketDriver
{
private:

	class LoLaSi446xRadioTaskClass : Task, public virtual IRadioTask
	{
	private:
		LoLaSi446xPacketDriver* Driver = nullptr;

		bool DriverEnabled = false;

	public:
		LoLaSi446xRadioTaskClass(Scheduler* scheduler, LoLaSi446xPacketDriver* driver) : Task(0, TASK_FOREVER, scheduler, false)
		{
			Driver = driver;
		}

		bool Callback()
		{
			if (!DriverEnabled || Driver->CheckPending())
			{
				disable();
			}

			return true;
		}

		virtual void Wake()
		{
			enableIfNot();
		}

		void Enable()
		{
			DriverEnabled = true;
			Wake();
		}

		void Disable()
		{
			DriverEnabled = false;
		}

	};

	LoLaSi446xRadioTaskClass RadioTask;

public:
	LoLaSi446xPacketTaskDriver(Scheduler* scheduler, SPIClass* spi, const uint8_t cs, const uint8_t reset, const uint8_t irq)
		: LoLaSi446xPacketDriver(spi, cs, reset, irq)
		, RadioTask(scheduler, this)
	{
	}

	bool Setup()
	{
		SetRadioTask(&RadioTask);

		return LoLaSi446xPacketDriver::Setup();
	}

protected:
	virtual void OnStart()
	{
		LoLaSi446xPacketDriver::OnStart();
		RadioTask.Enable();
	}

	virtual void OnStop()
	{
		RadioTask.Disable();
	}
};
#endif