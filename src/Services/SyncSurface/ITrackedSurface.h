// ITrackedSurface.h

#ifndef _ITRACKEDSURFACE_h
#define _ITRACKEDSURFACE_h

#include <BitTracker.h>
#include <Callback.h>
#include <FastCRC.h>

#define DEBUG_BIT_TRACKER

class ITrackedSurface
{
public:
	//4 bytes per block, enough for a 32 bit value (uint32_t, int32_t)
	static const uint8_t BytesPerBlock = 4;

private:
	//Callback handler.
	Signal<const bool> OnSurfaceUpdatedCallback;

	//Data good to use status.
	bool DataGood = false;

	uint8_t LastCRC = 0;
	boolean HashNeedsUpdate = true;

protected:
	//CRC Calculator.
	FastCRC8 CRC8;

protected:
	inline void OnDataChanged()
	{
		OnSurfaceUpdatedCallback.fire(DataGood);
	}

public:
	ITrackedSurface()
	{
		LastCRC = 0;
	}

	bool IsDataGood()
	{
		return DataGood;
	}

	void SetDataGood(const bool dataGood)
	{
		DataGood = dataGood;
	}

	void AttachOnSurfaceUpdated(const Slot<const bool>& slot)
	{
		OnSurfaceUpdatedCallback.attach(slot);
	}

	void NotifyDataChanged()
	{
		OnDataChanged();
	}

	void UpdateHash()
	{
		if (HashNeedsUpdate)
		{
			HashNeedsUpdate = false;
			LastCRC = CRC8.smbus(GetData(), GetDataSize());
		}
	}

	inline void InvalidateHash()
	{
		HashNeedsUpdate = true;
	}

	inline uint8_t GetHash()
	{
		return LastCRC;
	}

	inline uint8_t GetDataSize()
	{
		return GetBlockCount() * SYNC_SURFACE_BLOCK_SIZE;
	}

public:
	virtual uint8_t* GetData() { return nullptr; }
	virtual IBitTracker* GetTracker() { return nullptr; };
	inline virtual uint8_t GetBlockCount() { return 0; };
	virtual void SetAllPending() {};

public:
#ifdef DEBUG_BIT_TRACKER
	void Debug(Stream* serial)
	{
		serial->print(F("|"));
		for (uint8_t i = 0; i < (GetBlockCount() * SYNC_SURFACE_BLOCK_SIZE); i++)
		{
			serial->print(GetData()[i]);
			serial->print(F("|"));
		}
		serial->println();
	}

	virtual void PrintName(Stream* serial)
	{
		serial->print(F("ItrackedSurface"));
	}
#endif 
};

//BlockCount < 32
template <const uint8_t BlockCount>
class TemplateTrackedSurface : public ITrackedSurface
{
private:
	uint8_t IndexOffsetGrunt;

	TemplateBitTracker<BlockCount> Tracker;

protected:
	uint8_t Data[BlockCount * SYNC_SURFACE_BLOCK_SIZE];

protected:
	void InvalidateBlock(const uint8_t blockIndex)
	{
		Tracker.SetBit(blockIndex);
		InvalidateHash();
		OnDataChanged();
	}

protected:
	union ArrayToUint64 {
		byte array[sizeof(uint64_t)];
		uint64_t uint64;
		uint32_t uint32;
		uint16_t uint16;
	} ATUI;


public:
	TemplateTrackedSurface() : ITrackedSurface(), Tracker()
	{
		for (uint8_t i = 0; i < GetDataSize(); i++)
		{
			Data[i] = 0;
		}
		Tracker.ClearAll();
	}

	uint8_t* GetData()
	{
		return Data;
	}

	IBitTracker* GetTracker()
	{
		return &Tracker;
	}

	inline uint8_t GetBlockCount()
	{
		return BlockCount;
	}

	void SetAllPending()
	{
		Tracker.SetAll();
	}

	//Helper methods.
	//offset [0:3]
	inline uint8_t Get8(const uint8_t blockIndex, const uint8_t offset)
	{
		return Data[(blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset];
	}

	//offset [0:3]
	inline void Set8(const uint8_t value, const uint8_t blockIndex, const uint8_t offset)
	{
		Data[(blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset] = value;

		InvalidateBlock(blockIndex);
	}

	//offset [0:1]
	inline uint16_t Get16(const uint8_t blockIndex, const uint8_t offset)
	{
		IndexOffsetGrunt = (blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset * sizeof(uint16_t);

		ATUI.array[0] = Data[IndexOffsetGrunt + 0];
		ATUI.array[1] = Data[IndexOffsetGrunt + 1];

		return ATUI.uint16;
	}

	//offset [0:1]
	inline void Set16(const uint16_t value, const uint8_t blockIndex, const uint8_t offset)
	{
		IndexOffsetGrunt = (blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset * sizeof(uint16_t);

		ATUI.uint16 = value;
		Data[IndexOffsetGrunt + 0] = ATUI.array[0];
		Data[IndexOffsetGrunt + 1] = ATUI.array[1];

		InvalidateBlock(blockIndex);
	}

	inline uint32_t Get32(const uint8_t blockIndex)
	{
		IndexOffsetGrunt = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		ATUI.array[0] = Data[IndexOffsetGrunt + 0];
		ATUI.array[1] = Data[IndexOffsetGrunt + 1];
		ATUI.array[2] = Data[IndexOffsetGrunt + 2];
		ATUI.array[3] = Data[IndexOffsetGrunt + 3];

		return ATUI.uint32;
	}

	inline void Set32(const uint32_t value, const uint8_t blockIndex)
	{
		IndexOffsetGrunt = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		ATUI.uint32 = value;
		Data[IndexOffsetGrunt + 0] = ATUI.array[0];
		Data[IndexOffsetGrunt + 1] = ATUI.array[1];
		Data[IndexOffsetGrunt + 2] = ATUI.array[2];
		Data[IndexOffsetGrunt + 3] = ATUI.array[3];

		InvalidateBlock(blockIndex);
	}

	inline uint64_t Get64(const uint8_t blockIndex)
	{
		IndexOffsetGrunt = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		ATUI.array[0] = Data[IndexOffsetGrunt + 0];
		ATUI.array[1] = Data[IndexOffsetGrunt + 1];
		ATUI.array[2] = Data[IndexOffsetGrunt + 2];
		ATUI.array[3] = Data[IndexOffsetGrunt + 3];
		ATUI.array[4] = Data[IndexOffsetGrunt + 4];
		ATUI.array[5] = Data[IndexOffsetGrunt + 5];
		ATUI.array[6] = Data[IndexOffsetGrunt + 6];
		ATUI.array[7] = Data[IndexOffsetGrunt + 7];

		return ATUI.uint64;
	}

	inline void Get64(uint64_t& value, const uint8_t blockIndex)
	{
		IndexOffsetGrunt = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		ATUI.array[0] = Data[IndexOffsetGrunt + 0];
		ATUI.array[1] = Data[IndexOffsetGrunt + 1];
		ATUI.array[2] = Data[IndexOffsetGrunt + 2];
		ATUI.array[3] = Data[IndexOffsetGrunt + 3];
		ATUI.array[4] = Data[IndexOffsetGrunt + 4];
		ATUI.array[5] = Data[IndexOffsetGrunt + 5];
		ATUI.array[6] = Data[IndexOffsetGrunt + 6];
		ATUI.array[7] = Data[IndexOffsetGrunt + 7];

		value = ATUI.uint64;
	}

	inline void Set64(uint64_t& value, const uint8_t blockIndex)
	{
		IndexOffsetGrunt = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		ATUI.uint64 = value;
		Data[IndexOffsetGrunt + 0] = ATUI.array[0];
		Data[IndexOffsetGrunt + 1] = ATUI.array[1];
		Data[IndexOffsetGrunt + 2] = ATUI.array[2];
		Data[IndexOffsetGrunt + 3] = ATUI.array[3];
		Data[IndexOffsetGrunt + 4] = ATUI.array[4];
		Data[IndexOffsetGrunt + 5] = ATUI.array[5];
		Data[IndexOffsetGrunt + 6] = ATUI.array[6];
		Data[IndexOffsetGrunt + 7] = ATUI.array[7];

		InvalidateBlock(blockIndex);
		InvalidateBlock(blockIndex + 1);
	}
};
#endif