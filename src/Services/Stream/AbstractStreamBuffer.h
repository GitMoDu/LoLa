// AbstractStreamBuffer.h

#ifndef _ABSTRACTSTREAMBUFFER_h
#define _ABSTRACTSTREAMBUFFER_h


#include <Arduino.h>
#include <Services\IPacketSendService.h>

#include <Services\Stream\ITrackedStream.h>


class AbstractStreamBuffer : public IPacketSendService
{
private:

protected:
	ITrackedStream * TrackedStream = nullptr;

protected:
	virtual void OnWaitingForServiceDiscovery() {}

public:
	AbstractSync(Scheduler* scheduler, ILoLa* loLa, ITrackedStream* trackedStream, ILoLaPacket* packetHolder)
		: IPacketSendService(scheduler, 0, loLa, packetHolder)
	{
		TrackedStream = trackedStream;
	}

	bool OnEnable()
	{
		return true;
	}

	ITrackedStream* GetStream()
	{
		return TrackedStream;
	}

	void OnLinkEstablished()
	{
	}

	void OnLinkLost()
	{
	}

	void SurfaceDataChangedEvent(uint8_t param)
	{

	}

	void NotifyDataChanged()
	{
		if (TrackedStream != nullptr)
		{
			TrackedStream->NotifyDataChanged();
		}
	}

protected:
	virtual bool OnSetup()
	{
		if (IPacketSendService::OnSetup() && TrackedSurface != nullptr)
		{
			MethodSlot<AbstractSync, uint8_t> memFunSlot(this, &AbstractSync::SurfaceDataChangedEvent);
			TrackedSurface->AttachOnSurfaceUpdated(memFunSlot);
			return true;
		}

		return false;
	}

	void OnService()
	{
	}

};
#endif

