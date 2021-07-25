// AbstractStream.h

#ifndef _ABSTRACT_STREAM_h
#define _ABSTRACT_STREAM_h

#define STREAM_RETRANSMIT

#include <Arduino.h>
#include <Services\IPacketSendService.h>
#include <RingBufCPP.h>
#include <Services\Stream\ITrackedStream.h>

#define ABSTRACT_STREAM_RETRY_PERIOD					((uint32_t)50)
#define ABSTRACT_STREAM_REPLY_CHECK_PERIOD				(ABSTRACT_STREAM_RETRY_PERIOD*2)

template <class DataType, const uint8_t BufferSize>
class AbstractStreamBuffer : public IPacketSendService
{
private:
	uint32_t LastSent = ILOLA_INVALID_MILLIS;
	TemplateTrackedStreamBuffer <DataType, BufferSize> TrackedBuffer;

	DataType* Grunt;
protected:
	ITrackedStream * TrackedStream = nullptr;

	enum StreamStateEnum
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
			TrackedStream->AttachOnNewDataAvailableCallback(memFunSlot);
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

template <class DataType, const uint8_t BufferSize, const uint8_t CacheSize>
class AbstractStreamReader : public AbstractStreamBuffer<DataType, BufferSize>
{
private:
	RingBufCPP<DataType, CacheSize> SideBuffer;

	DataType ReaderGrunt;

public:
	AbstractStreamReader(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedStream* trackedStream)
		: AbstractStream(scheduler, loLa, baseHeader, trackedStream)
	{
	}

private:
	bool IsStreamContinuous()
	{
		return true;//TODO:
	}

	uint8_t GetMissingStreamId()
	{
		return 0;//TODO:
	}

	void PrepareRequestRetransmitPacket(const uint8_t id)
	{
		//TODO:
	}

protected:
	virtual bool DecodePayload(DataType * target, uint8_t[] payload)
	{
		return false;//TODO:
	}


protected:

	void ProcessStreamPacket(uint8_t[] payload)
	{
		switch (StreamState)
		{
		case StreamStateEnum::WaitingForServiceDiscovery:
			StreamState = StreamStateEnum::Active;
			Enable();
			SetNextRunASAP();
		case StreamStateEnum::Active:
			if (DecodePayload(&ReaderGrunt, payload))
			{
				TrackedStream.AddNew(ReaderGrunt);
			}
			else
			{
				//TODO:
			}
			break;
		case SyncStateEnum::Disabled:
			break;
		default:
			break;
		}
	}

	void OnWaitingForServiceDiscovery()
	{
		if (LastSent != ILOLA_INVALID_MILLIS ||
			Millis() - LastSent > ABSTRACT_STREAM_RETRY_PERIOD)
		{
			PrepareServiceDiscoveryPacket();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(ABSTRACT_STREAM_REPLY_CHECK_PERIOD);
		}
	}

	void OnStreamActive()
	{
		if (IsStreamContinuous())
		{
			SetNextRunDelay(1000);
		}
		else
		{
			uint8_t MissingId = GetMissingStreamId();
			PrepareRequestRetransmitPacket(MissingId);
			RequestSendPacket();
		}
	}
};


template <class DataType, const uint8_t BufferSize, const uint8_t CacheSize>
class AbstractStreamWriter : public AbstractStreamBuffer<DataType, BufferSize>
{
public:
	AbstractStreamReader(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedStream* trackedStream)
		: AbstractStream(scheduler, loLa, baseHeader, trackedStream)
	{
	}
private:
#ifdef STREAM_RETRANSMIT

	template <class DataType, const uint8_t CacheSize>
	class SearcheableRingBufCPP
	{
	private:
		RingBufCPP<DataType, CacheSize> DataList;

	public:

		inline void AddNew(DataType * newData)
		{
			DataList.forcePush(*newData);
		}
		//TODO: Doesn't work unless DataType has explicit GetId()
		DataType * FindCached(const uint8_t id)
		{
			/*	for (uint8_t i = 0; i < DataList.numElements(); i++)
				{
					if (DataList.peek(i)->GetId() == id)
					{
						return DataList.peek(i);
					}
				}*/

			return nullptr;
		}
	};


	SearcheableRingBufCPP<DataType, CacheSize> RetransmitCache;

	RingBufCPP<uint8_t, CacheSize> RetransmitRequests;

	uint8_t RetransmitRequestId;
#endif

protected:
	virtual void ProcessStreamPacket(DataType * newData);

private:
#ifdef STREAM_RETRANSMIT

	bool AllowedRetransmit()
	{
		return TrackedStream.GetCount() < (BufferSize / 2);
	}
#endif

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

#ifdef STREAM_RETRANSMIT
	void OnRetransmitRequestReceived(const uint8_t requestedId)
	{
		switch (StreamState)
		{
		case StreamStateEnum::WaitingForServiceDiscovery:
			StreamState = StreamStateEnum::Active;
			Enable();
			SetNextRunASAP();
			break;
		case StreamStateEnum::Active:
			RetransmitRequests.forcePush(requestedId);
			Enable();
			SetNextRunASAP();
			break;
		case SyncStateEnum::Disabled:
			break;
		default:
			break;
		}
	}
#endif

	void OnStreamActive()
	{
#ifdef STREAM_RETRANSMIT
		if(!RetransmitRequests.isEmpty())
		{
			if (AllowedRetransmit())
			{
				RetransmitRequests.pull(RetransmitRequestId);

				Grunt = RetransmitCache.FindCached(RetransmitRequestId);

				if (Grunt != nullptr)
				{
					PrepareStreamPacket(Grunt);
					RequestSendPacket();
				}
			}
			else
			{
				//Clear requests;
				while (!RetransmitRequests.isEmpty())
				{
					RetransmitRequests.pull();
				}
			}
		}
		else 
#endif
		if (TrackedStream.HasData())
		{
			Grunt = TrackedStream->PullOldest();
			PrepareStreamPacket(Grunt);
			RequestSendPacket();
			RetransmitCache.AddNew(Grunt);
		}
		else
		{
			SetNextRunDelay(1000);
		}
		//TODO:
	}
};

#endif

