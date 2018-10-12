// AbstractStream.h

#ifndef _ABSTRACT_STREAM_h
#define _ABSTRACT_STREAM_h


#include <Arduino.h>
#include <Services\IPacketSendService.h>

#include <Services\Stream\ITrackedStream.h>


class AbstractStream : public IPacketSendService
{
private:

protected:
	ITrackedStream * TrackedStream = nullptr;

	enum StreamStateEnum : uint8_t
	{
		WaitingForServiceDiscovery = 0,
		Active = 1,
		Disabled = 2
	} StreamState = StreamStateEnum::Disabled;

protected:
	virtual void OnWaitingForServiceDiscovery() {}
	virtual void OnNewDataAvailable() {}
	virtual void Clear() {};

	virtual void OnStreamActive() {}

public:
	AbstractStream(Scheduler* scheduler, ILoLa* loLa, ITrackedStream* trackedStream, ILoLaPacket* packetHolder)
		: IPacketSendService(scheduler, 0, loLa, packetHolder)
	{
		TrackedStream = trackedStream;

		StreamState = StreamStateEnum::Disabled;
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
		Enable();
	}

	void OnLinkLost()
	{
		Disable();
	}

	void OnNewDataAvailableEvent(uint8_t param)
	{
		OnNewDataAvailable();
	}

protected:
	virtual bool OnSetup()
	{
		if (IPacketSendService::OnSetup() && TrackedStream != nullptr)
		{
			MethodSlot<AbstractSync, uint8_t> memFunSlot(this, &AbstractStreamBuffer::OnNewDataAvailableEvent);
			TrackedSurface->AttachOnNewDataAvailableCallback(memFunSlot);
			return true;
		}

		return false;
	}

	inline void NotifyDataChanged()
	{
		if (TrackedStream != nullptr)
		{
			TrackedStream->NotifyDataChanged();
		}
	}

	void Clear()
	{
		TrackedStream->Clear();
	}

	void OnService()
	{
		switch (StreamState)
		{
		case StreamStateEnum::WaitingForServiceDiscovery:
			OnWaitingForServiceDiscovery();
			break;
		case StreamStateEnum::Active:
			OnStreamActive();
			break;
		case StreamStateEnum::Disabled:
		default:
			SetNextRunLong();
			break;
		}
	}

};
#endif

