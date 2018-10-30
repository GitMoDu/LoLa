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

	void OnStateUpdated(const SyncStateEnum newState)
	{
		switch (newState)
		{
		case SyncStateEnum::Syncing:
			TrackedSurface->GetTracker()->SetAll();
			break;
		case SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->ClearAll();
			InvalidateLocalHash();
			UpdateLocalHash();
			break;
		default:
			break;
		}
	}

	void OnWaitingForServiceDiscovery()
	{
		if (GetElapsedSinceStateStart() > ABSTRACT_SURFACE_MAX_ELAPSED_NO_DISCOVERY_MILLIS)
		{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.println(F("Sync Surface: No service found."));
#endif
			Disable();
		}
		else if (GetElapsedSinceLastSent() > ABSTRACT_SURFACE_SERVICE_DISCOVERY_SEND_PERIOD)
		{
			PrepareServiceDiscoveryPacket();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(ABSTRACT_SURFACE_FAST_CHECK_PERIOD_MILLIS);
		}
	}

	void OnSyncActive()
	{
		SetNextRunDelay(ABSTRACT_SURFACE_SLOW_CHECK_PERIOD_MILLIS);
	}

	void OnUpdateFinishedReceived()
	{
		UpdateLocalHash();

		switch (SyncState)
		{
		case SyncStateEnum::Syncing:
			if (HashesMatch())
			{
				PrepareUpdateFinishedReplyPacket();
				RequestSendPacket();
				UpdateSyncState(SyncStateEnum::Synced);
			}
			else
			{
				PrepareInvalidateRequestPacket();
				RequestSendPacket();
			}
			break;
		case SyncStateEnum::Synced:
			if (HashesMatch())
			{
				PrepareUpdateFinishedReplyPacket();
				RequestSendPacket();
			}
			else
			{
				PrepareInvalidateRequestPacket();
				RequestSendPacket();
				UpdateSyncState(SyncStateEnum::Syncing);
			}

			//TODO: Add resync sub-state, where the reader just asks the writer to start again.
			break;
		default:
			break;
		}
	}
};
#endif