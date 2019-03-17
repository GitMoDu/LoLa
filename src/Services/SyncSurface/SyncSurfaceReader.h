// SyncSurfaceReader.h

#ifndef _SYNCSURFACEREADER_h
#define _SYNCSURFACEREADER_h

#include <Services\SyncSurface\SyncSurfaceBase.h>

template <const uint8_t BasePacketHeader>
class SyncSurfaceReader : public SyncSurfaceBase
{
private:
	SyncMetaPacketDefinition<BasePacketHeader> SyncMetaDefinition;
	SyncDataPacketDefinition<BasePacketHeader> DataPacketDefinition;

public:
	SyncSurfaceReader(Scheduler* scheduler, ILoLaDriver* driver, ITrackedSurface* trackedSurface)
		: SyncSurfaceBase(scheduler, driver, trackedSurface, &SyncMetaDefinition, &DataPacketDefinition)
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
			TrackedSurface->SetDataGood(false);
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

	void OnStateUpdated(const AbstractSync::SyncStateEnum newState)
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
			if (!TrackedSurface->IsDataGood())
			{
				TrackedSurface->SetDataGood(true);
			}
			break;
		case SyncStateEnum::Disabled:
			TrackedSurface->SetDataGood(false);
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
			SetNextRunDelay(ABSTRACT_SURFACE_SERVICE_DISCOVERY_SEND_PERIOD - GetElapsedSinceLastSent());
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
			PrepareUpdateFinishedReplyPacket();
			RequestSendPacket();
			if (HashesMatch())
			{
				UpdateSyncState(SyncStateEnum::Synced);
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
			break;
		case SyncStateEnum::WaitingForServiceDiscovery:
			PrepareUpdateFinishedReplyPacket();
			RequestSendPacket();
			if (HashesMatch())
			{
				UpdateSyncState(SyncStateEnum::Synced);
			}
			else
			{
				UpdateSyncState(SyncStateEnum::Syncing);
			}
			break;
		default:
			break;
		}
	}
};
#endif