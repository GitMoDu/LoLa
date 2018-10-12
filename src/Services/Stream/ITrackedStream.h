// ITrackedStream.h

#ifndef _ITRACKED_STREAM_h
#define _ITRACKED_STREAM_h

#include <BitTracker.h>
#include <Callback.h>

#include <RingBufCPP.h>


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
		OnDataChanged();
	}

public:
	virtual bool HasData()
	{
		return false;
	}

protected:
	inline void OnNewDataAvailable()
	{
		OnNewDataAvailableCallback.fire(0);
	}
};


template <const uint8_t BufferSize, class DataType>
class TemplateTrackedStreamBuffer : public ITrackedStream
{
private:
	RingBufCPP<uintDataType16_t, BufferSize> CircularBuffer;

	DataType Grunt;

public:
	DataType GetOldest()
	{
		if (!CircularBuffer.isEmpty())
		{
			CircularBuffer.pull(Grunt);
		}
		else
		{
			Grunt = 0;
		}

		return Grunt;
	}

	void AddNew(const DataType newValue)
	{
		CircularBuffer.addForce(newValue);
		OnNewDataAvailable();
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

