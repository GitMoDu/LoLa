// ISurface.h

#ifndef _I_SURFACE_h
#define _I_SURFACE_h

#include <stdint.h>

class ISurfaceListener
{
public:
	/// <summary>
	/// Notifies the listener that the surface data has been updated.
	/// </summary>
	/// <param name="hot">Same as ISurface::IsHot().</param>
	virtual void OnSurfaceUpdated(const bool hot) {}
};

/// <summary>
/// A Surface is composed of [0;255] 8 byte blocks.
/// Blocks are big enough for a 64 bit value (uint64_t, int64_t)
/// and each one has an accompanying Pending flag.
/// </summary>
class ISurface
{
public:
	static constexpr uint8_t BytesPerBlock = 8;
	static constexpr uint8_t BitsPerBlock = BytesPerBlock * 8;

public:
	static constexpr uint8_t GetByteCount(const uint8_t blockCount)
	{
		return blockCount * BytesPerBlock;
	}

public:
	/// <summary>
	/// </summary>
	/// <returns>The Surface block count.</returns>
	virtual const uint8_t GetBlockCount() { return 0; }

	/// <summary>
	/// </summary>
	/// <returns>Pointer to raw block data.</returns>
	virtual uint8_t* GetBlockData() { return nullptr; }

	/// <summary>
	/// Notifies the listener that the data has been updated.
	/// </summary>
	virtual void NotifyUpdated() {}

	/// <summary>
	/// Attach a listener for OnSurfaceUpdated event.
	/// </summary>
	/// <param name="listener"></param>
	/// <returns>True if success.</returns>
	virtual const bool AttachSurfaceListener(ISurfaceListener* listener) { return false; }

	/// <summary>
	/// Abstract surface Status flag.
	/// </summary>
	/// <returns></returns>
	virtual const bool IsHot() { return false; }

	/// <summary>
	/// </summary>
	/// <param name="hot">Abstract surface status.</param>
	virtual void SetHot(const bool hot) { }

	/// <summary>
	/// Flag all blocks as pending.
	/// </summary>
	virtual void SetAllBlocksPending() {}

	/// <summary>
	/// Clear all blocks' pending flags.
	/// </summary>
	virtual void ClearAllBlocksPending() {}

	/// <summary>
	/// Clear a block's pending flags.
	/// </summary>
	virtual void ClearBlockPending(const uint8_t blockIndex) {}

	/// <summary>
	/// Flag a block as pending.
	/// </summary>
	virtual void SetBlockPending(const uint8_t blockIndex) {}

	/// <summary> 
	/// </summary>
	/// <param name="blockIndex">Index  [0 ; GetBlockCount()-1]</param>
	/// <returns>True if a block as pending.</returns>
	virtual const bool IsBlockPending(const uint8_t blockIndex) { return false; }

	/// <summary>
	/// </summary>
	/// <returns>True if any block as pending.</returns>
	virtual const bool HasBlockPending() { return false; }

	/// <summary>
	/// </summary>
	/// <param name="startingIndex">Search start offset.</param>
	/// <returns>Index of the next pending block, 0 if none found</returns>
	virtual const uint8_t GetNextBlockPendingIndex(const uint8_t startingIndex) { return 0; }

#if defined(DEBUG_LOLA)
	virtual void Debug(Stream* serial) {}
#endif
};
#endif