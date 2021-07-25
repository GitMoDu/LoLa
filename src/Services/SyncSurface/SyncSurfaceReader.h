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
	SyncSurfaceReader(Scheduler* scheduler, LoLaPacketDriver* driver, ITrackedSurface* trackedSurface)
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
		serial->print(F("SurfaceReader ("));
		TrackedSurface->PrintName(serial);
		serial->print(F(")"));
	}
#endif

protected:
	void OnBlockReceived(const uint8_t index, uint8_t* payload)
	{
		switch (SyncState)
		{
		case AbstractSync::SyncStateEnum::Syncing:
			UpdateBlockData(index, payload);
			TrackedSurface->ClearBlockPending(index);
			TrackedSurface->NotifyDataChanged();
			LastUpdateReceived = millis();
			break;
		case AbstractSync::SyncStateEnum::WaitingForServiceDiscovery:
		case AbstractSync::SyncStateEnum::Synced:
			UpdateBlockData(index, payload);
			TrackedSurface->SetAllPending();
			TrackedSurface->ClearBlockPending(index);
			TrackedSurface->NotifyDataChanged();
			LastUpdateReceived = millis();
			UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			break;
		case AbstractSync::SyncStateEnum::Disabled:
		default:
			break;
		}
	}

	void OnStateUpdated(const AbstractSync::SyncStateEnum newState)
	{
		switch (newState)
		{
		case AbstractSync::SyncStateEnum::Synced:
			TrackedSurface->StampDataIntegrity();
			break;
		case AbstractSync::SyncStateEnum::Disabled:
			TrackedSurface->ResetDataIntegrity();
			break;
		default:
			break;
		}
	}

	void OnWaitingForServiceDiscovery()
	{
		if (GetElapsedSinceStateStart() > MAX_ELAPSED_NO_DISCOVERY_MILLIS)
		{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.println(F("Sync Surface: No service found."));
#endif
			UpdateSyncState(AbstractSync::SyncStateEnum::Disabled);
		}
		else if (GetElapsedSinceLastSent() > DISCOVERY_SEND_PERIOD)
		{
			SendServiceDiscoveryPacket();
		}
		else
		{
			SetNextRunDelay(DISCOVERY_SEND_PERIOD - GetElapsedSinceLastSent());
		}
	}

	void OnSyncActive()
	{
		bool SendInvalidate = false;

		if (CheckThrottling())
		{
			if (GetElapsedSinceStateStart() > SLOW_CHECK_PERIOD_MILLIS
				&& (LastUpdateReceived == 0 ||
					(millis() - LastUpdateReceived > RECEIVE_FAILED_PERIOD)))
			{
				SendInvalidateRequestPacket();
			}
		}
		else
		{
			SetNextRunDelay(FAST_CHECK_PERIOD_MILLIS);
		}
	}

	void OnUpdateFinishedReceived()
	{
		switch (SyncState)
		{
		case AbstractSync::SyncStateEnum::Syncing:
			SendUpdateFinishedReplyPacket();
			if (HashesMatch())
			{
				UpdateSyncState(AbstractSync::SyncStateEnum::Synced);
			}
			LastUpdateReceived = millis();
			break;
		case AbstractSync::SyncStateEnum::Synced:
			if (HashesMatch())
			{
				SendUpdateFinishedReplyPacket();
			}
			else
			{
				SendInvalidateRequestPacket();
				UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			}
			LastUpdateReceived = millis();
			break;
		case AbstractSync::SyncStateEnum::WaitingForServiceDiscovery:
			SendInvalidateRequestPacket();
			TrackedSurface->SetAllPending();
			UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			break;
		default:
			break;
		}
	}

private:
	void SendServiceDiscoveryPacket()
	{
		SendMetaPacket(SYNC_META_SUB_HEADER_SERVICE_DISCOVERY, GetSurfaceId());
	}

	void SendUpdateFinishedReplyPacket()
	{
		SendMetaHashPacket(SYNC_META_SUB_HEADER_UPDATE_FINISHED_REPLY);
	}

	void SendInvalidateRequestPacket()
	{
		SendMetaHashPacket(SYNC_META_SUB_HEADER_INVALIDATE_REQUEST);
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
		Serial.println(F("Surface Reader Recovery!"));
#endif
	}

	void UpdateBlockData(const uint8_t blockIndex, uint8_t* payload)
	{
		InvalidateLocalHash();

		uint8_t IndexOffset = blockIndex * ITrackedSurface::BytesPerBlock;

		for (uint8_t i = 0; i < ITrackedSurface::BytesPerBlock; i++)
		{
			SurfaceData[IndexOffset + i] = payload[1 + 1 + i];
		}
	}
};
#endif