// SyncSurfaceBlock.h

#ifndef _SYNC_SURFACE_BLOCK_h
#define _SYNC_SURFACE_BLOCK_h



#include <Arduino.h>
#include <Services\SyncSurface\AbstractSync.h>



class SyncSurfaceBlock : public AbstractSync
{
private:
	uint8_t IndexOffset = 0;

public:
	SyncSurfaceBlock(Scheduler* scheduler, const uint16_t period, ILoLa* loLa, ITrackedSurfaceNotify* trackedSurface)
		: AbstractSync(scheduler, period, loLa, trackedSurface)
	{
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

	void PrepareBlockPacketPayload(const uint8_t index, uint8_t * payload)
	{
		IndexOffset = index * SYNC_SURFACE_BLOCK_SIZE;

		for (uint8_t i = 0; i < SYNC_SURFACE_BLOCK_SIZE; i++)
		{
			payload[i] = TrackedSurface->GetData()[IndexOffset + i];
		}
	}

	virtual void OnStateUpdated(const SyncStateEnum newState)
	{
		AbstractSync::OnStateUpdated(newState);
		switch (newState)
		{
		case SyncStateEnum::Starting:
		case SyncStateEnum::WaitingForTrigger:
		case SyncStateEnum::FullSync:
		case SyncStateEnum::Synced:
		case SyncStateEnum::Resync:
		case SyncStateEnum::Disabled:
		default:
			break;
		}
	}
};
#endif