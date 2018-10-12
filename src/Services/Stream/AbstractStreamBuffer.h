// AbstractStream.h

#ifndef _ABSTRACT_STREAM_h
#define _ABSTRACT_STREAM_h


#include <Arduino.h>
#include <Services\IPacketSendService.h>

#include <Services\Stream\ITrackedStream.h>

#define ABSTRACT_STREAM_RETRY_PERIDO					((uint32_t)50)
#define ABSTRACT_STREAM_REPLY_CHECK_PERIOD				(ABSTRACT_STREAM_RETRY_PERIDO*2)

class AbstractStreamReader : public AbstractStream
{
public:
	AbstractStreamReader(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedStream* trackedStream)
		: AbstractStream(scheduler, loLa, baseHeader, trackedStream)
	{
	}

protected:
	void OnWaitingForServiceDiscovery()
	{
		if (LastSent != ILOLA_INVALID_MILLIS ||
			Millis() - LastSent > ABSTRACT_STREAM_RETRY_PERIDO)
		{
			PrepareServiceDiscoveryPacket();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(ABSTRACT_STREAM_REPLY_CHECK_PERIOD);
		}
	}
};

class AbstractStreamWriter : public AbstractStream
{
public:
	AbstractStreamReader(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedStream* trackedStream)
		: AbstractStream(scheduler, loLa, baseHeader, trackedStream)
	{
	}

protected:
	void OnServiceDiscoveryReceived()
	{
		switch (StreamState)
		{
		case StreamStateEnum::WaitingForServiceDiscovery:
			StreamState = StreamStateEnum::Active;
			Enable();
			SetNextRunASAP();
			break;
		case StreamStateEnum::Active:
			break;
		case SyncStateEnum::Disabled:
			break;
		default:
			break;
		}
	}

	void OnStreamActive() 
	{
		//TODO:
	}
};

class AbstractStream : public IPacketSendService
{
private:
	uint32_t LastSent = ILOLA_INVALID_MILLIS;


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
		StreamState = StreamStateEnum::WaitingForServiceDiscovery;
		SetNextRunASAP();
	}

	void OnLinkLost()
	{
		Disable();
		StreamState = StreamStateEnum::Disabled;
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

