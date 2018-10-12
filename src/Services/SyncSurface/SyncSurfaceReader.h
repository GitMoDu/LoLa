// SyncSurfaceReader.h

#ifndef _SYNCSURFACEREADER_h
#define _SYNCSURFACEREADER_h

#include <Services\SyncSurface\SyncSurfaceBase.h>

class SyncSurfaceReader : public SyncSurfaceBase
{
public:
	SyncSurfaceReader(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedSurface* trackedSurface)
		: SyncSurfaceBase(scheduler, loLa, baseHeader, trackedSurface)
	{
	}

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncSurfaceReader"));
	}
#endif // DEBUG_LOLA

	void OnBlockReceived(const uint8_t index, uint8_t * payload)
	{
		UpdateBlockData(index, payload);
		InvalidateLocalHash();
		TrackedSurface->GetTracker()->ClearBit(index);
		NotifyDataChanged();

		switch (SyncState)
		{
		case SyncStateEnum::WaitingForServiceDiscovery:
			UpdateSyncState(SyncStateEnum::Syncing);
			break;
		case SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->SetAll();
			TrackedSurface->GetTracker()->ClearBit(index);
			UpdateSyncState(SyncStateEnum::Syncing);
			break;
		default:
			break;
		}
	}

	void OnSyncStateUpdated(const SyncStateEnum newState)
	{
		switch (newState)
		{
		case SyncStateEnum::Syncing:
			TrackedSurface->GetTracker()->SetAll();
			TrackedSurface->GetTracker()->Debug(&Serial);
			break;
		case SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->ClearAll();
			TrackedSurface->GetTracker()->Debug(&Serial);
			break;
		default:
			break;
		}
	}

	void OnWaitingForServiceDiscovery()
	{
		if (GetElapsedSinceStateStart() > ABSTRACT_SURFACE_MAX_ELAPSED_DATA_SYNC_LOST)
		{
			Disable();
		}
		else if (GetElapsedSinceLastSent() > ABSTRACT_SURFACE_SYNC_RETRY_PERIDO)
		{
			PrepareServiceDiscoveryPacket();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(ABSTRACT_SURFACE_SYNC_RETRY_PERIDO);
		}
	}

	void OnSyncActive()
	{
		if (GetElapsedSinceLastReceived() > ABSTRACT_SURFACE_MAX_ELAPSED_DATA_SYNC_LOST)
		{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.print(F("WaitingForDataUpdate Timeout. Elapsed since last received: "));
			Serial.print(GetElapsedSinceLastReceived());
#endif
			UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
		}
		else
		{
			InvalidateLocalHash();
			UpdateLocalHash();
			SetNextRunDelay(ABSTRACT_SURFACE_SYNC_REPLY_CHECK_PERIOD);
		}
	}

	void OnUpdateFinishedReceived()
	{
		switch (SyncState)
		{
		case SyncStateEnum::Syncing:
			UpdateLocalHash();
			PrepareUpdateFinishedReplyPacket();
			RequestSendPacket();
			if (HashesMatch())
			{
				UpdateSyncState(SyncStateEnum::Synced);
			}
			break;
		case SyncStateEnum::Synced:
			UpdateLocalHash();
			PrepareUpdateFinishedReplyPacket();
			RequestSendPacket();

			if (!HashesMatch())
			{
				//TODO: Replace with explicit invalidation packet.
				UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
			}

			//TODO: Add resync sub-state, where the reader just asks the writer to start again.
			break;
		default:
			break;
		}
	}
};
#endif