// ITrackedSurface.h

#ifndef _ITRACKEDSURFACE_h
#define _ITRACKEDSURFACE_h

#include <BitTracker.h>
#include <Callback.h>
#include <Crypto\TinyCRC.h>

#define SYNC_SURFACE_BLOCK_SIZE 4 //4 bytes per block, enough for a 32 bit value (uint32_t, int32_t);

#define SYNC_SURFACE_8_SIZE		32	//8*LOLA_SYNC_SURFACE_BLOCK_SIZE
#define SYNC_SURFACE_16_SIZE	64	//16*LOLA_SYNC_SURFACE_BLOCK_SIZE
#define SYNC_SURFACE_32_SIZE	128	//32*LOLA_SYNC_SURFACE_BLOCK_SIZE
#define SYNC_SURFACE_64_SIZE	256	//64*LOLA_SYNC_SURFACE_BLOCK_SIZE
#define SYNC_SURFACE_128_SIZE	512	//128*LOLA_SYNC_SURFACE_BLOCK_SIZE

class ITrackedSurface
{
private:
	//Callback handler.
	Signal<uint8_t> OnSurfaceUpdatedCallback;

	//CRC Calculator.
	TinyCrcModbus8 CalculatorCRC;
	boolean HashNeedsUpdate = true;

protected:
	inline void OnDataChanged()
	{
		OnSurfaceUpdatedCallback.fire(0);
	}

public:
	ITrackedSurface()
	{
		CalculatorCRC.Reset();
	}

	void AttachOnSurfaceUpdated(const Slot<uint8_t>& slot)
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
			CalculatorCRC.Reset();

			for (uint8_t i = 0; i < GetDataSize(); i++)
			{
				CalculatorCRC.Update(GetData()[i]);
			}
		}
	}

	inline void InvalidateHash()
	{
		HashNeedsUpdate = true;
	}

	inline uint8_t GetHash()
	{
		return CalculatorCRC.GetCurrent();
	}

public:
	virtual uint8_t* GetData() { return nullptr; }
	virtual IBitTracker* GetTracker() { return nullptr; };
	inline virtual uint8_t GetSize() const { return 0; };
	inline virtual uint8_t GetDataSize() const { return 0; };
	virtual void SetAllPending() {};


protected:
	//Helper methods.
	inline uint16_t Get8(const uint8_t blockIndex, const uint8_t offset = 0);
	inline void Set8(const uint8_t blockIndex, const uint8_t value, const uint8_t offset = 0);
	inline uint16_t Get16(const uint8_t blockIndex, const uint8_t offset = 0);
	inline void Set16(const uint8_t blockIndex, const uint16_t value, const uint8_t offset = 0);
	inline uint32_t Get32(const uint8_t blockIndex);
	inline void Set32(const uint8_t blockIndex, const uint32_t value);

public:
#ifdef DEBUG_LOLA
	void Debug(Stream * serial)
	{
		serial->print(F("|"));
		for (uint8_t i = 0; i < (GetSize()*SYNC_SURFACE_BLOCK_SIZE); i++)
		{
			serial->print(GetData()[i]);
			serial->print(F("|"));
		}
		serial->println();
	}
#endif 
};

//BlockCount < 32
template <uint8_t BlockCount>
class TemplateTrackedSurface : public ITrackedSurface
{
private:
	const uint8_t DataSize = BlockCount * SYNC_SURFACE_BLOCK_SIZE;
	uint8_t IndexOffsetGrunt;

	TemplateBitTracker<BlockCount> Tracker;
	uint8_t Data[BlockCount * SYNC_SURFACE_BLOCK_SIZE];

private:
	void InvalidateBlock(const uint8_t blockIndex)
	{
		Tracker.SetBit(blockIndex);
		OnDataChanged();
	}

public:
	TemplateTrackedSurface() : ITrackedSurface()
	{
		for (uint8_t i = 0; i < DataSize; i++)
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

	inline uint8_t GetSize() const
	{
		return BlockCount;
	}

	inline uint8_t GetDataSize() const
	{
		return DataSize;
	}

	void SetAllPending()
	{
		Tracker.SetAll();
	}

	//Helper methods.
	//offset [0:3]
	inline uint16_t Get8(const uint8_t blockIndex, const uint8_t offset = 0)
	{
		return GetData()[(blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset];
	}

	//offset [0:3]
	inline void Set8(const uint8_t value, const uint8_t blockIndex, const uint8_t offset = 0)
	{
		GetData()[(blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset] = value & 0x00FF;

		InvalidateBlock(blockIndex);
	}

	//offset [0:1]
	inline uint16_t Get16(const uint8_t blockIndex, const uint8_t offset = 0)
	{
		IndexOffsetGrunt = (blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset*2;
		return GetData()[IndexOffsetGrunt] + ((uint16_t)GetData()[IndexOffsetGrunt + 1] << 8);
	}

	//offset [0:1]
	inline void Set16(const uint16_t value, const uint8_t blockIndex, const uint8_t offset = 0)
	{
		IndexOffsetGrunt = (blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset*2;
		GetData()[IndexOffsetGrunt] = value & 0x00FF;
		GetData()[IndexOffsetGrunt + 1] = value >> 8;

		InvalidateBlock(blockIndex);
	}

	inline uint32_t Get32(const uint8_t blockIndex)
	{
		IndexOffsetGrunt = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		uint32_t Grunt32 = GetData()[IndexOffsetGrunt];
		Grunt32 += (uint16_t)GetData()[IndexOffsetGrunt + 1] << 8;
		Grunt32 += (uint32_t)GetData()[IndexOffsetGrunt + 2] << 16;
		Grunt32 += (uint32_t)GetData()[IndexOffsetGrunt + 3] << 24;

		return Grunt32;
	}

	inline void Set32(const uint32_t value, const uint8_t blockIndex)
	{
		IndexOffsetGrunt = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		GetData()[IndexOffsetGrunt] = value & 0x00FF;
		GetData()[IndexOffsetGrunt + 1] = value >> 8;
		GetData()[IndexOffsetGrunt + 2] = value >> 16;
		GetData()[IndexOffsetGrunt + 3] = value >> 24;

		InvalidateBlock(blockIndex);
	}
};
#endif
