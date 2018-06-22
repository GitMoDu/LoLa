// ITrackedSurface.h

#ifndef _ITRACKEDSURFACE_h
#define _ITRACKEDSURFACE_h

#include <BitTracker.h>
#include <Callback.h>

#define SYNC_SURFACE_BLOCK_SIZE 4 //4 bytes per block, enough for a 32 bit value (uint32_t, int32_t);

#define SYNC_SURFACE_8_SIZE		32	//8*LOLA_SYNC_SURFACE_BLOCK_SIZE
#define SYNC_SURFACE_16_SIZE	64	//16*LOLA_SYNC_SURFACE_BLOCK_SIZE
#define SYNC_SURFACE_32_SIZE	128	//32*LOLA_SYNC_SURFACE_BLOCK_SIZE
#define SYNC_SURFACE_64_SIZE	256	//64*LOLA_SYNC_SURFACE_BLOCK_SIZE
#define SYNC_SURFACE_128_SIZE	512	//128*LOLA_SYNC_SURFACE_BLOCK_SIZE

class ITrackedSurface
{
private:
	uint8_t DataSize = 0;
	uint8_t IndexOffsetGrunt;

private:
	void InvalidateBlock(const uint8_t blockIndex)
	{
		GetTracker()->SetBitPending(blockIndex);
	}
	
protected:
	virtual void OnDataChanged()
	{
	}

public:
	ITrackedSurface()
	{
	}

	void Initialize()
	{
		DataSize = GetTracker()->GetSize() * SYNC_SURFACE_BLOCK_SIZE;
		for (uint8_t i = 0; i < GetSize(); i++)
		{
			GetData()[i] = 0;			
		}
		GetTracker()->ClearAllPending();
	}

	void Invalidate()
	{
		OnDataChanged();
	}

	uint8_t GetSize()
	{
		return GetTracker()->GetSize();
	}

	uint8_t GetDataSize()
	{
		return DataSize;
	}

	void SetAllPending()
	{
		for (uint8_t i = 0; i < GetSize(); i++)
		{
			GetTracker()->SetBitPending(i);
		}
	}

	//Helper methods.
	//offset [0:3]
	inline uint16_t Get8(const uint8_t blockIndex, const uint8_t offset = 0)
	{
		return GetData()[(blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset];
	}

	//offset [0:3]
	inline void Set8(const uint8_t blockIndex, const uint8_t value, const uint8_t offset = 0)
	{
		GetData()[(blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset] = value & 0x00FF;

		InvalidateBlock(blockIndex);
		OnDataChanged();
	}

	//offset [0:1]
	inline uint16_t Get16(const uint8_t blockIndex, const uint8_t offset = 0)
	{
		IndexOffsetGrunt = (blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset*2;
		return GetData()[IndexOffsetGrunt] + ((uint16_t)GetData()[IndexOffsetGrunt + 1] << 8);
	}

	//offset [0:1]
	inline void Set16(const uint8_t blockIndex, const uint16_t value, const uint8_t offset = 0)
	{
		IndexOffsetGrunt = (blockIndex * SYNC_SURFACE_BLOCK_SIZE) + offset*2;
		GetData()[IndexOffsetGrunt] = value & 0x00FF;
		GetData()[IndexOffsetGrunt + 1] = value >> 8;

		InvalidateBlock(blockIndex);
		OnDataChanged();
	}

	uint32_t Get32(const uint8_t blockIndex)
	{
		IndexOffsetGrunt = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		uint32_t Grunt32 = GetData()[IndexOffsetGrunt];
		Grunt32 += (uint16_t)GetData()[IndexOffsetGrunt + 1] << 8;
		Grunt32 += (uint32_t)GetData()[IndexOffsetGrunt + 2] << 16;
		Grunt32 += (uint32_t)GetData()[IndexOffsetGrunt + 3] << 24;

		return Grunt32;
	}

	void Set32(const uint8_t blockIndex, const uint32_t value)
	{
		IndexOffsetGrunt = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		GetData()[IndexOffsetGrunt] = value & 0x00FF;
		GetData()[IndexOffsetGrunt + 1] = value >> 8;
		GetData()[IndexOffsetGrunt + 2] = value >> 16;
		GetData()[IndexOffsetGrunt + 3] = value >> 24;

		InvalidateBlock(blockIndex);
		OnDataChanged();
	}

public:
	virtual uint8_t* GetData() { return nullptr; }
	virtual IBitTracker* GetTracker() { return nullptr; }

#ifdef DEBUG_LOLA
	void Debug(Stream * serial)
	{
		serial->print(F("|"));
		for (uint8_t i = 0; i < GetSize(); i++)
		{
			serial->print(GetData()[i]);
			serial->print(F("|"));
		}
		serial->println();
	}
#endif 
};

class ITrackedSurfaceNotify : public ITrackedSurface
{
private:
	//Callback handler
	Signal<uint8_t> OnSurfaceUpdatedCallback;

protected:
	void OnDataChanged()
	{
		OnSurfaceUpdatedCallback.fire(0);
	}

public:
	ITrackedSurfaceNotify()
		: ITrackedSurface()
	{
	}

	//Public interface
	void AttachOnSurfaceUpdated(const Slot<uint8_t>& slot)
	{
		OnSurfaceUpdatedCallback.attach(slot);
	}

	void NotifyDataChanged()
	{
		OnDataChanged();
	}
};
#endif
