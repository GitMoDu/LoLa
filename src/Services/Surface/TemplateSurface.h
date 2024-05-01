// TemplateSurface.h

#ifndef _TEMPLATE_SURFACE_h
#define _TEMPLATE_SURFACE_h


/*
* https://github.com/GitMoDu/BitTracker
*/
#include <BitTracker.h>
#include "ISurface.h"

/// <summary>
/// Abstract Template class implements
///		Static allocation of the data array and the bit tracker.
///		Bit tracking for each block.
///		Interface listeners management.
/// </summary>
/// <param name="BlockCount">[0 ; UINT8_MAX]</param>
template<const uint8_t BlockCount, const uint8_t MaxListenerCount = 10>
class TemplateSurface : public ISurface
{
private:
	uint8_t* BlockData;

	TemplateBitTracker<uint8_t, BlockCount> BlockTracker{};

	ISurfaceListener* Listeners[MaxListenerCount]{};

	uint8_t ListenerCount = 0;

	bool Hot = false;

public:
	TemplateSurface(uint8_t* blockData)
		: ISurface()
		, BlockData(blockData)
	{}

	/// <summary>
	/// Notifies the block tracker that a data block has been updated.
	/// </summary>
	/// <param name="blockIndex">Index  [0 ; BlockCount-1]</param>
	/// <param name="notify">If true, will fire NotifyUpdated.
	/// False is used when updating more than one block and only firing the event at the last one.</param>
	void OnBlockUpdated(const uint8_t blockIndex, const bool notify = true)
	{
		BlockTracker.SetBit(blockIndex);

		if (notify)
		{
			NotifyUpdated();
		}
	}

	/// <summary>
	/// ISurface implementation.
	/// </summary>
public:
	virtual const uint8_t GetBlockCount() final
	{
		return BlockCount;
	}

	virtual uint8_t* GetBlockData() final
	{
		return BlockData;
	}

	virtual const bool IsHot() final
	{
		return Hot;
	}

	virtual void SetHot(const bool hot) final
	{
		Hot = hot;
	}

	virtual void SetAllBlocksPending() final
	{
		BlockTracker.SetAll();
	}

	virtual void ClearAllBlocksPending() final
	{
		BlockTracker.ClearAll();
	}

	virtual void ClearBlockPending(const uint8_t blockIndex) final
	{
		BlockTracker.ClearBit(blockIndex);
	}

	virtual void SetBlockPending(const uint8_t blockIndex) final
	{
		BlockTracker.SetBit(blockIndex);
	}

	virtual const bool IsBlockPending(const uint8_t blockIndex) final
	{
		return BlockTracker.IsBitSet(blockIndex);
	}

	virtual const bool HasBlockPending() final
	{
		return BlockTracker.HasSet();
	}

	virtual const uint8_t GetNextBlockPendingIndex(const uint8_t startingIndex) final
	{
		return BlockTracker.GetNextSetIndex(startingIndex);
	}

	virtual const bool AttachSurfaceListener(ISurfaceListener* listener) final
	{
		if ((listener != nullptr) && (ListenerCount < MaxListenerCount))
		{
			Listeners[ListenerCount] = listener;
			ListenerCount++;

			return true;
		}
		else
		{
			return false;
		}
	}

	virtual void NotifyUpdated() final
	{
		for (uint_fast8_t i = 0; i < ListenerCount; i++)
		{
			Listeners[i]->OnSurfaceUpdated(Hot);
		}
	}

#if defined(DEBUG_TRACKED_SURFACE)
	virtual void Debug(Stream* serial) final
	{
		serial->print(F("|||"));
		for (uint_fast8_t i = 0; i < BlockCount; i++)
		{
			for (uint_fast8_t j = 0; j < BytesPerBlock; j++)
			{
				serial->print(BlockData[(i * BytesPerBlock) + j]);
				serial->print(F("|"));
			}
			serial->print(F("|"));
		}
		serial->print(F("|"));
		serial->println();
	}
#endif
};
#endif