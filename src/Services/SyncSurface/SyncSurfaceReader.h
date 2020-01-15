// SyncSurfaceReader.h

#ifndef _SYNCSURFACEREADER_h
#define _SYNCSURFACEREADER_h

#include <Services\SyncSurface\SyncSurfaceBase.h>


template <const uint8_t BasePacketHeader,
	const uint32_t ThrottlePeriodMillis = 1>
	class SyncSurfaceReader : public SyncSurfaceBase
{
private:
	SyncMetaPacketDefinition<BasePacketHeader> SyncMetaDefinition;
	SyncDataPacketDefinition<BasePacketHeader> DataPacketDefinition;

	uint32_t LastUpdateReceived = 0;

public:
	SyncSurfaceReader(Scheduler* scheduler, ILoLaDriver* driver, ITrackedSurface* trackedSurface)
		: SyncSurfaceBase(scheduler, driver, trackedSurface, ThrottlePeriodMillis)
		, SyncMetaDefinition(this)
		, DataPacketDefinition(this)
	{
		MetaDefinition = &SyncMetaDefinition;
		DataDefinition = &DataPacketDefinition;
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncSurfaceReader"));
	}
#endif

protected:
	void OnBlockReceived(const uint8_t index, uint8_t* payload)
	{
		UpdateBlockData(index, payload);
		InvalidateLocalHash();
		TrackedSurface->GetTracker()->ClearBit(index);
		NotifyDataChanged();

		switch (SyncState)
		{
		case AbstractSync::SyncStateEnum::WaitingForServiceDiscovery:
			UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			TrackedSurface->SetDataGood(false);
			break;
		case AbstractSync::SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->SetAll();
			TrackedSurface->GetTracker()->ClearBit(index);
			UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			break;
		default:
			break;
		}

		LastUpdateReceived = millis();
	}

	void OnStateUpdated(const AbstractSync::SyncStateEnum newState)
	{
		switch (newState)
		{
		case AbstractSync::SyncStateEnum::Syncing:
			LastUpdateReceived = 0;
			TrackedSurface->GetTracker()->SetAll();
			break;
		case AbstractSync::SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->ClearAll();
			InvalidateLocalHash();
			UpdateLocalHash();
			if (!TrackedSurface->IsDataGood())
			{
				TrackedSurface->SetDataGood(true);
				NotifyDataChanged();
			}
			break;
		case AbstractSync::SyncStateEnum::Disabled:
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
		if (LastUpdateReceived != 0 &&
			(millis() - LastUpdateReceived > ABSTRACT_SURFACE_RECEIVE_FAILED_PERIDO) &&
			CheckThrottling() &&
			GetElapsedSinceLastSent() > ABSTRACT_SURFACE_SYNC_CONFIRM_SEND_PERIOD_MILLIS)
		{
			PrepareInvalidateRequestPacket();
			RequestSendPacket();
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.println(F("Surface Reader Recovery!"));
#endif
		}
		else
		{
			SetNextRunDelay(ABSTRACT_SURFACE_FAST_CHECK_PERIOD_MILLIS);
		}

	}

	void OnUpdateFinishedReceived()
	{
		UpdateLocalHash();

		switch (SyncState)
		{
		case AbstractSync::SyncStateEnum::Syncing:
			PrepareUpdateFinishedReplyPacket();
			RequestSendPacket();
			if (HashesMatch())
			{
				UpdateSyncState(AbstractSync::SyncStateEnum::Synced);
			}
			break;
		case AbstractSync::SyncStateEnum::Synced:
			if (HashesMatch())
			{
				PrepareUpdateFinishedReplyPacket();
				RequestSendPacket();
			}
			else
			{
				PrepareInvalidateRequestPacket();
				RequestSendPacket();
				UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			}
			break;
		case AbstractSync::SyncStateEnum::WaitingForServiceDiscovery:
			PrepareUpdateFinishedReplyPacket();
			RequestSendPacket();
			if (HashesMatch())
			{
				UpdateSyncState(AbstractSync::SyncStateEnum::Synced);
			}
			else
			{
				UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			}
			break;
		default:
			break;
		}
	}
};
#endif