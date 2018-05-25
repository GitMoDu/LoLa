// SyncSurfaceBlock.h

#ifndef _SYNC_SURFACE_BLOCK_h
#define _SYNC_SURFACE_BLOCK_h



#include <Arduino.h>
#include <Services\SyncSurface\AbstractSync.h>
#include <Services\SyncSurface\ITrackedSurface.h>


class SyncSurfaceBlock : public AbstractSync
{
protected:
	ITrackedSurfaceNotify * TrackedSurface = nullptr;

	//Process to fire external events.

public:
	SyncSurfaceBlock(Scheduler* scheduler, const uint16_t period, ILoLa* loLa, ITrackedSurfaceNotify* trackedSurface)
		: AbstractSync(scheduler, period, loLa)
	{
		TrackedSurface = trackedSurface;
	}

	ITrackedSurface* GetSurface()
	{
		return TrackedSurface;
	}

private:
	uint8_t IndexOffset = 0;

protected:
	virtual bool OnSetup()
	{
		if (AbstractSync::OnSetup() && TrackedSurface != nullptr)
		{
			TrackedSurface->Initialize();
			if (TrackedSurface->GetData() != nullptr
				&& TrackedSurface->GetTracker() != nullptr
				&& TrackedSurface->GetSize() > 0
				&& TrackedSurface->GetSize() <= TrackedSurface->GetTracker()->GetBitCount())
			{
				MethodSlot<SyncSurfaceBlock, uint8_t> memFunSlot(this, &SyncSurfaceBlock::SurfaceDataChanged);
				TrackedSurface->AttachOnSurfaceUpdated(memFunSlot);
				return true;
			}			
		}

		return false;
	}

protected:
	void UpdateBlockData(const uint8_t blockIndex, uint8_t * payload)
	{
		IndexOffset = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		for (uint8_t i = 0; i < SYNC_SURFACE_BLOCK_SIZE; i++)
		{
			TrackedSurface->GetData()[IndexOffset + i] = payload[i];
		}
	}

	void PrepareBlockPayload(const uint8_t index, uint8_t * payload)
	{
		IndexOffset = index * SYNC_SURFACE_BLOCK_SIZE;

		for (uint8_t i = 0; i < SYNC_SURFACE_BLOCK_SIZE; i++)
		{
			payload[i] = TrackedSurface->GetData()[IndexOffset + i];
		}
	}

	void UpdateHash()
	{
		AbstractSync::UpdateHash();

		for (uint8_t i = 0; i < TrackedSurface->GetDataSize(); i++)
		{
			CalculatorCRC.Update(TrackedSurface->GetData()[i]);
		}
	}

protected:
	virtual void OnSurfaceDataChanged(){}

public:
	void SurfaceDataChanged(uint8_t param)
	{
		OnSurfaceDataChanged();
	}

	void NotifyDataChanged()
	{
		if (TrackedSurface != nullptr)
		{
			TrackedSurface->NotifyDataChanged();
		}
	}
};
#endif