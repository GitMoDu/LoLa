// SyncSurfaceWriter.h

#ifndef _SYNCSURFACEWRITER_h
#define _SYNCSURFACEWRITER_h


#include <Services\SyncSurface\SyncSurfaceBase.h>

template <const uint8_t BasePacketHeader,
	const uint32_t ThrottlePeriodMillis = 1>
	class SyncSurfaceWriter : public SyncSurfaceBase
{
private:
	SyncMetaPacketDefinition<BasePacketHeader> SyncMetaDefinition;
	SyncDataPacketDefinition<BasePacketHeader> DataPacketDefinition;


public:
	SyncSurfaceWriter(Scheduler* scheduler, ILoLaDriver* driver, ITrackedSurface* trackedSurface)
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
		serial->print(F("SyncSurfaceWriter"));
	}
#endif

private:
	enum SyncWriterState : uint8_t
	{
		UpdatingBlocks = 0,
		SendingBlock = 1,
		SendingFinished = 2
	} WriterState = SyncWriterState::UpdatingBlocks;

	uint8_t SurfaceSendingIndex = 0;

protected:
	void OnTransmitted(const uint8_t header, const uint8_t id, const uint32_t transmitDuration, const uint32_t sendDuration)
	{
		if (SyncState == AbstractSync::SyncStateEnum::Syncing && WriterState == SyncWriterState::SendingBlock)
		{
			TrackedSurface->GetTracker()->ClearBit(SurfaceSendingIndex);
			SurfaceSendingIndex++;
		}
		SetNextRunASAP();
	}

	void OnStateUpdated(const AbstractSync::SyncStateEnum newState)
	{
		switch (newState)
		{
		case AbstractSync::SyncStateEnum::WaitingForServiceDiscovery:
			InvalidateRemoteHash();
			Disable();
		case AbstractSync::SyncStateEnum::Syncing:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			break;
		default:
			break;
		}
	}

	void OnServiceDiscoveryReceived(const uint8_t surfaceId)
	{
		if (surfaceId != GetSurfaceId())
		{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.println(F("Surface Version mismatch"));
#endif // DEBUG_LOLA

			return;
		}

		switch (SyncState)
		{
		case AbstractSync::SyncStateEnum::Syncing:
			break;
		case AbstractSync::SyncStateEnum::Disabled:
			break;
		default:
			UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			break;
		}
	}

	void OnWaitingForServiceDiscovery()
	{
		Disable();
	}

	void OnUpdateFinishedReplyReceived()
	{
		UpdateLocalHash();

		switch (SyncState)
		{
		case AbstractSync::SyncStateEnum::WaitingForServiceDiscovery:
			break;
		case AbstractSync::SyncStateEnum::Syncing:
			switch (WriterState)
			{
			case SyncWriterState::SendingFinished:
				if (TrackedSurface->GetTracker()->HasSet())
				{
					UpdateSyncingState(SyncWriterState::UpdatingBlocks);
				}
				else if (HashesMatch())
				{
					UpdateSyncState(AbstractSync::SyncStateEnum::Synced);
				}
				else
				{
					TrackedSurface->GetTracker()->SetAll();
					UpdateSyncingState(SyncWriterState::UpdatingBlocks);
				}
				break;
			default:
				break;
			}
			break;
		case AbstractSync::SyncStateEnum::Synced:
			if (!HashesMatch())
			{
				TrackedSurface->GetTracker()->SetAll();
				UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			}
			break;
		case AbstractSync::SyncStateEnum::Disabled:
			break;
		default:
			break;
		}
	}

	void OnInvalidateRequestReceived()
	{
		switch (SyncState)
		{
		case AbstractSync::SyncStateEnum::WaitingForServiceDiscovery:
			break;
		case AbstractSync::SyncStateEnum::Syncing:
			TrackedSurface->GetTracker()->SetAll();
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			break;
		case AbstractSync::SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->SetAll();
			UpdateSyncState(AbstractSync::SyncStateEnum::Syncing);
			break;
		case AbstractSync::SyncStateEnum::Disabled:
			break;
		default:
			break;
		}
	}

	void OnSyncActive()
	{
		switch (WriterState)
		{
		case SyncWriterState::UpdatingBlocks:
			if (TrackedSurface->GetTracker()->HasSet())
			{
				if (CheckThrottling())
				{
					PrepareNextPendingBlockPacket();
					RequestSendPacket();
					UpdateSyncingState(SyncWriterState::SendingBlock);
				}
				else
				{
					SetNextRunDelay(ABSTRACT_SURFACE_FAST_CHECK_PERIOD_MILLIS);
				}
			}
			else
			{
				UpdateSyncingState(SyncWriterState::SendingFinished);
				ResetLastSentTimeStamp();
				SetNextRunDelay(ABSTRACT_SURFACE_UPDATE_BACK_OFF_PERIOD_MILLIS);
			}
			break;
		case SyncWriterState::SendingBlock:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			SetNextRunDelay(ABSTRACT_SURFACE_FAST_CHECK_PERIOD_MILLIS);
			break;
		case SyncWriterState::SendingFinished:
			if (TrackedSurface->GetTracker()->HasSet())
			{
				UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			}
			else if (GetElapsedSinceLastSent() > ABSTRACT_SURFACE_SYNC_CONFIRM_SEND_PERIOD_MILLIS)
			{
				PrepareUpdateFinishedPacket();
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(ABSTRACT_SURFACE_SYNC_CONFIRM_SEND_PERIOD_MILLIS - GetElapsedSinceLastSent());
			}
			break;
		default:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			break;
		}
	}

	void OnSendDelayed()
	{
		if (SyncState == AbstractSync::SyncStateEnum::Syncing && WriterState == SyncWriterState::SendingBlock)
		{
			UpdatePendingBlockPacketPayload();//Update packet payload with latest live data.
		}
	}

	void OnSendRetrying()
	{
		if (SyncState == AbstractSync::SyncStateEnum::Syncing && WriterState == SyncWriterState::SendingBlock)
		{
			UpdatePendingBlockPacketPayload();//Update packet payload with latest live data.
		}
	}

private:
	void UpdateSyncingState(const SyncWriterState newState)
	{
		if (WriterState != newState)
		{
			SetNextRunASAP();
			ResetLastSentTimeStamp();

#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.print(LoLaDriver->GetMicros());
			Serial.print(F(": Updated Writer Syncing to "));
#endif
			switch (newState)
			{
			case SyncWriterState::UpdatingBlocks:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("UpdatingBlocks"));
#endif
				break;
			case SyncWriterState::SendingBlock:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("SendingBlock"));
#endif
				InvalidateRemoteHash();
				break;
			case SyncWriterState::SendingFinished:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("SendingFinished"));
#endif
				break;
			default:
				break;
			}

			WriterState = newState;
		}
	}

	inline void UpdatePendingBlockPacketPayload()
	{
		PrepareBlockPacketPayload(SurfaceSendingIndex, Packet->GetPayload());
	}

	void PrepareNextPendingBlockPacket()
	{
		if (SurfaceSendingIndex >= TrackedSurface->GetBlockCount())
		{
			SurfaceSendingIndex = 0;
		}
		SurfaceSendingIndex = TrackedSurface->GetTracker()->GetNextSetIndex(SurfaceSendingIndex);
		PrepareBlockPacketHeader(SurfaceSendingIndex);
		PrepareBlockPacketPayload(SurfaceSendingIndex, Packet->GetPayload());
	}
};
#endif
