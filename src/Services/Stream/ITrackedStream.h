// ITrackedStream.h

#ifndef _ITRACKED_STREAM_h
#define _ITRACKED_STREAM_h

#include <BitTracker.h>
#include <Callback.h>

// https://github.com/rlogiacco/CircularBuffer
#include <CircularBuffer.h>


class ITrackedStream
{
private:
	//Callback handler.
	Signal<uint8_t> OnNewDataAvailableCallback;

public:
	ITrackedStream() {}

	void AttachOnNewDataAvailableCallback(const Slot<uint8_t>& slot)
	{
		OnNewDataAvailableCallback.attach(slot);
	}

	void NotifyDataChanged()
	{
		OnNewDataAvailable();
	}

public:
	virtual bool HasData()
	{
		return false;
	}

	virtual uint8_t GetChunckSize()
	{
		return 0;
	}

protected:
	inline void OnNewDataAvailable()
	{
		OnNewDataAvailableCallback.fire(0);
	}
};


template <class DataType, const uint8_t BufferSize>
class TemplateTrackedStreamBuffer : public ITrackedStream
{
private:

	CircularBuffer<DataType, BufferSize> CircularBuffer;

	DataType Grunt;

public:
	DataType * PullOldest()
	{
		if (!CircularBuffer.isEmpty())
		{
			CircularBuffer.pull(Grunt);
		}
		else
		{
			return nullptr;
		}

		return &Grunt;
	}

	uint8_t GetChunckSize() const
	{
		return sizeof(DataType);
	}
	 
	void AddNew(const DataType newValue)
	{
		CircularBuffer.addForce(newValue);
		//This should be optimized to only check on a set flag.
		OnNewDataAvailable();
	}

	inline uint8_t GetCount()
	{
		return CircularBuffer.numElements();
	}

	bool HasData()
	{
		return !CircularBuffer.isEmpty();
	}

	void Clear()
	{
		while (!CircularBuffer.isEmpty())
		{
			CircularBuffer.pull();
		}
	}
};
#endif

