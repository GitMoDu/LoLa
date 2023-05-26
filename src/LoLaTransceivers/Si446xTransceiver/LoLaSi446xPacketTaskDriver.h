//// LoLaSi446xPacketTaskDriver.h
//
//#ifndef _LOLASI446XPACKETTASKDRIVER_h
//#define _LOLASI446XPACKETTASKDRIVER_h
//
//
//#include <PacketDriver\LoLaSi446x\LoLaSi446xPacketDriver.h>
//#include <PacketDriver\LoLaSi446x\IRadioTask.h>
//
//class LoLaSi446xPacketTaskDriver : public LoLaSi446xPacketDriver
//{
//private:
//
//	class LoLaSi446xRadioTaskClass : Task, public virtual IRadioTask
//	{
//	private:
//		LoLaSi446xPacketDriver* Driver = nullptr;
//
//	public:
//		LoLaSi446xRadioTaskClass(Scheduler* scheduler, LoLaSi446xPacketDriver* driver) : Task(0, TASK_FOREVER, scheduler, false)
//		{
//			Driver = driver;
//		}
//
//		bool Callback()
//		{
//			if (Driver->CheckPending())
//			{
//				disable();
//			}
//
//			return true;
//		}
//
//		virtual void Wake()
//		{
//			enableIfNot();
//		}
//
//	};
//
//	LoLaSi446xRadioTaskClass RadioTask;
//
//public:
//	LoLaSi446xPacketTaskDriver(Scheduler* scheduler, SPIClass* spi, const uint8_t cs, const uint8_t reset, const uint8_t irq)
//		: LoLaSi446xPacketDriver(spi, cs, reset, irq)
//		, RadioTask(scheduler, this)
//	{
//	}
//
//	bool Setup()
//	{
//		SetRadioTask(&RadioTask);
//
//		return LoLaSi446xPacketDriver::Setup();
//	}
//
//protected:
//	virtual void OnStart() 
//	{
//		RadioTask.Wake();
//	}
//};
//
//
//#endif
//
