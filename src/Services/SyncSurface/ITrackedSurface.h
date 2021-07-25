// ITrackedSurface.h

#ifndef _ITRACKEDSURFACE_h
#define _ITRACKEDSURFACE_h

//#define DEBUG_TRACKED_SURFACE

#include <BitTracker.h>
#include <Callback.h>
#include <FastCRC.h>



class ITrackedSurface
{
public:
	// 8 bytes per block, enough for a 64 bit value (uint64_t, int64_t)
	static const uint8_t BytesPerBlock = 8;
	static const uint8_t BitsPerBlock = BytesPerBlock * 8;

private:
	// Callback handler.
	Signal<const bool> OnSurfaceUpdatedCallback;

	// Surface has been completely synced at least once.
	bool OneGoodSync = false;

	// Block tracker.
	IBitTracker* BlockTracker = nullptr;

public:
	// Surface data.
	uint8_t* SurfaceData = nullptr;

	// Constants.
	const uint16_t DataSize;
	const uint8_t BlockCount;


public:
	ITrackedSurface(uint8_t* surfaceData, IBitTracker* blockTracker, const uint8_t blockCount)
		: SurfaceData(surfaceData)
		, BlockTracker(blockTracker)
		, BlockCount(blockCount)
		, DataSize(blockCount* BytesPerBlock)
	{
	}

	void AttachOnSurfaceUpdated(const Slot<const bool>& slot)
	{
		OnSurfaceUpdatedCallback.attach(slot);
	}

	void NotifyDataChanged()
	{
		OnDataChanged();
	}

	void StampDataIntegrity()
	{
		if (!OneGoodSync)
		{
			OneGoodSync = true;
			OnDataChanged();
		}
	}

	void ResetDataIntegrity()
	{
		OneGoodSync = false;
	}

	bool HasSynced()
	{
		return OneGoodSync;
	}

	IBitTracker* GetBlockTracker()
	{
		return BlockTracker;
	}

	void SetAllPending()
	{
		BlockTracker->SetAll();
	}

	void ClearAllPending()
	{
		BlockTracker->ClearAll();
	}

	void ClearBlockPending(const uint8_t blockIndex)
	{
		BlockTracker->ClearBit(blockIndex);
	}

	bool HasAnyPending()
	{
		return BlockTracker->HasSet();
	}

	uint8_t GetNextSetPendingIndex(const uint8_t startingIndex)
	{
		if (startingIndex < BlockCount)
		{
			return (uint8_t)BlockTracker->GetNextSetIndex(startingIndex);
		}
		else
		{
			return 0;
		}
	}

protected:
	void InvalidateBlock(const uint8_t blockIndex, const bool notify = true)
	{
		BlockTracker->SetBit(blockIndex);
		if (notify)
		{
			OnDataChanged();
		}
	}

	void OnDataChanged()
	{
		if (OneGoodSync)
		{
			OnSurfaceUpdatedCallback.fire(false);
		}
	}

public:
#ifdef DEBUG_TRACKED_SURFACE
	void Debug(Stream* serial)
	{
		serial->print(F("|||"));
		for (uint8_t i = 0; i < BlockCount; i++)
		{
			for (uint8_t j = 0; j < BytesPerBlock; j++)
			{
				serial->print(SurfaceData[(i * BytesPerBlock) + j]);
				serial->print(F("|"));
			}
			serial->print(F("|"));
		}
		serial->print(F("|"));
		serial->println();
	}

	virtual void PrintName(Stream* serial)
	{
		serial->print(F("ItrackedSurface"));
	}
#endif 
};

// Block count <= 255
template <const uint8_t Blocks>
class TemplateTrackedSurface : public ITrackedSurface
{
public:
	static const uint8_t ByteCount = Blocks * BytesPerBlock;

private:
	TemplateBitTracker<Blocks> Tracker;

protected:
	union ArrayToUint64 {
		uint8_t array[sizeof(uint64_t)];
		uint64_t uint64;
	};

	union ArrayToUint32 {
		uint8_t array[sizeof(uint32_t)];
		uint32_t uint32;
	};

	union ArrayToUint16 {
		uint8_t array[sizeof(uint16_t)];
		uint16_t uint16;
	};

	// First block is dedicated to binary options.
	static const uint8_t BINARY_BLOCK_ADDRESS = 0;

	uint8_t Data[ByteCount];

public:
	TemplateTrackedSurface()
		: ITrackedSurface(Data, &Tracker, Blocks)
		, Tracker()
	{
	}

protected:
	uint8_t GetU8(const uint8_t blockIndex, const uint8_t offset)
	{
		return Data[(blockIndex * BytesPerBlock) + offset];
	}

	int8_t GetS8(const uint8_t blockIndex, const uint8_t offset)
	{
		return (int8_t)(GetU8(blockIndex, offset) - INT8_MAX);
	}

	void SetU8(const uint8_t value, const uint8_t blockIndex, const uint8_t offset)
	{
		Data[(blockIndex * BytesPerBlock) + offset] = value;

		InvalidateBlock(blockIndex);
	}

	void SetS8(const int8_t value, const uint8_t blockIndex, const uint8_t offset)
	{
		SetU8((uint8_t)(value + INT8_MAX), blockIndex, offset);
	}

	uint16_t GetU16(const uint8_t blockIndex, const uint8_t offset)
	{
		const uint8_t IndexOffsetGrunt = (blockIndex * BytesPerBlock) + offset;

		ArrayToUint16 ATUI;

		ATUI.array[0] = Data[IndexOffsetGrunt + 0];
		ATUI.array[1] = Data[IndexOffsetGrunt + 1];

		return ATUI.uint16;
	}

	int16_t GetS16(const uint8_t blockIndex, const uint8_t offset)
	{
		return (int16_t)(GetU16(blockIndex, offset) - INT16_MAX);
	}

	void SetU16(const uint16_t value, const uint8_t blockIndex, const uint8_t offset)
	{
		const uint8_t IndexOffsetGrunt = (blockIndex * BytesPerBlock) + offset;

		ArrayToUint16 ATUI;

		ATUI.uint16 = value;
		Data[IndexOffsetGrunt + 0] = ATUI.array[0];
		Data[IndexOffsetGrunt + 1] = ATUI.array[1];

		InvalidateBlock(blockIndex);
	}

	void SetS16(const int16_t value, const uint8_t blockIndex, const uint8_t offset)
	{
		SetU16((uint16_t)(value + INT16_MAX), blockIndex, offset);
	}

	uint32_t GetU32(const uint8_t blockIndex, const uint8_t offset)
	{
		const uint8_t IndexOffsetGrunt = (blockIndex * BytesPerBlock) + offset;

		ArrayToUint32 ATUI;

		ATUI.array[0] = Data[IndexOffsetGrunt + 0];
		ATUI.array[1] = Data[IndexOffsetGrunt + 1];
		ATUI.array[2] = Data[IndexOffsetGrunt + 2];
		ATUI.array[3] = Data[IndexOffsetGrunt + 3];

		return ATUI.uint32;
	}

	uint32_t GetS32(const uint8_t blockIndex, const uint8_t offset)
	{
		return (int32_t)(GetU32(blockIndex, offset) - INT32_MAX);
	}

	void SetU32(const uint32_t value, const uint8_t blockIndex, const uint8_t offset)
	{
		const uint8_t IndexOffsetGrunt = (blockIndex * BytesPerBlock) + offset;

		ArrayToUint32 ATUI;

		ATUI.uint32 = value;
		Data[IndexOffsetGrunt + 0] = ATUI.array[0];
		Data[IndexOffsetGrunt + 1] = ATUI.array[1];
		Data[IndexOffsetGrunt + 2] = ATUI.array[2];
		Data[IndexOffsetGrunt + 3] = ATUI.array[3];

		InvalidateBlock(blockIndex);
	}

	void SetS32(const int16_t value, const uint8_t blockIndex, const uint8_t offset)
	{
		SetU32((uint32_t)(value + INT32_MAX), blockIndex, offset);
	}

	uint64_t GetU64(const uint8_t blockIndex)
	{
		const uint8_t IndexOffsetGrunt = blockIndex * BytesPerBlock;

		ArrayToUint64 ATUI;

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

	void GetU64(uint64_t& value, const uint8_t blockIndex)
	{
		const uint8_t IndexOffsetGrunt = blockIndex * BytesPerBlock;

		ArrayToUint64 ATUI;

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

	void SetU64(uint64_t value, const uint8_t blockIndex)
	{
		const uint8_t IndexOffsetGrunt = blockIndex * BytesPerBlock;

		ArrayToUint64 ATUI;

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
	}

	void GetS64(int64_t& value, const uint8_t blockIndex)
	{
		value = (int64_t)GetS64(blockIndex);
	}

	int64_t GetS64(const uint8_t blockIndex)
	{
		return (int64_t)(GetU64(blockIndex) - INT64_MAX);
	}

	void SetS64(int64_t value, const uint8_t blockIndex)
	{
		SetU64((uint64_t)(value + INT64_MAX));
	}
};
#endif