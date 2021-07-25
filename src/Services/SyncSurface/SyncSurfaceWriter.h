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
	SyncSurfaceWriter(Scheduler* scheduler, LoLaPacketDriver* driver, ITrackedSurface* trackedSurface)
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
		serial->print(F("SurfaceWriter ("));
		TrackedSurface->PrintName(serial);
		serial->print(F(")"));
	}
#endif

private:
	enum SyncWriterState
	{
		UpdatingBlocks = 0,
		SendingBlock = 1,
		SendingFinished = 2
	} WriterState = SyncWriterState::UpdatingBlocks;

	uint8_t SurfaceSendingIndex = 0;

protected:
	void OnSendOk(const uint8_t header, const uint8_t id, const uint32_t transmitDuration, const uint32_t sendDuration)
	{
		if (SyncState == AbstractSync::SyncStateEnum::Syncing && WriterState == SyncWriterState::SendingBlock)
		{
			TrackedSurface->ClearBlockPending(SurfaceSendingIndex);
			SurfaceSendingIndex++; // Forces next block even if current block is quickly set pending from outside.
		}
		SetNextRunASAP();
	}

	void OnStateUpdated(const AbstractSync::SyncStateEnum newState)
	{
		switch (newState)
		{
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
		switch (SyncState)
		{
		case AbstractSync::SyncStateEnum::WaitingForServiceDiscovery:
			break;
		case AbstractSync::SyncStateEnum::Syncing:
			switch (WriterState)
			{
			case SyncWriterState::SendingFinished:
				if (TrackedSurface->HasAnyPending())
				{
					UpdateSyncingState(SyncWriterState::UpdatingBlocks);
				}
				else if (HashesMatch())
				{
					UpdateSyncState(AbstractSync::SyncStateEnum::Synced);
				}
				else
				{
					TrackedSurface->SetAllPending();
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
				TrackedSurface->SetAllPending();
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
			TrackedSurface->SetAllPending();
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			break;
		case AbstractSync::SyncStateEnum::Synced:
			TrackedSurface->SetAllPending();
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
			if (TrackedSurface->HasAnyPending())
			{
				if (CheckThrottling())
				{
					SendNextPendingBlockPacket();
					UpdateSyncingState(SyncWriterState::SendingBlock);
				}
				else
				{
					SetNextRunDelay(FAST_CHECK_PERIOD_MILLIS);
				}
			}
			else
			{
				UpdateSyncingState(SyncWriterState::SendingFinished);
				ResetLastSentTimeStamp();
				SetNextRunDelay(UPDATE_BACK_OFF_PERIOD_MILLIS);
			}
			break;
		case SyncWriterState::SendingBlock:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			SetNextRunDelay(FAST_CHECK_PERIOD_MILLIS);
			break;
		case SyncWriterState::SendingFinished:
			if (TrackedSurface->HasAnyPending())
			{
				UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			}
			else if (GetElapsedSinceLastSent() > SYNC_CONFIRM_RESEND_PERIOD_MILLIS)
			{
				SendUpdateFinishedPacket();
			}
			else
			{
				SetNextRunDelay(SYNC_CONFIRM_RESEND_PERIOD_MILLIS - GetElapsedSinceLastSent());
			}
			break;
		default:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			break;
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
				ForceUpdateLocalHash();
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

	void UpdatePendingBlockPacketPayload()
	{
		GetOutPayload()[1] = GetLocalHash();
		FillBlockPacketData(OutMessage[0]);
	}

	void SendNextPendingBlockPacket()
	{
		SurfaceSendingIndex = TrackedSurface->GetNextSetPendingIndex(SurfaceSendingIndex);
		SendBlockPacketHeader(SurfaceSendingIndex);
		
	}

	// BlockPacketPayload is called separately because we can do last minute update to payload.
	void FillBlockPacketData(const uint8_t index)
	{
		const uint8_t IndexOffset = index * ITrackedSurface::BytesPerBlock;

		for (uint8_t i = 0; i < ITrackedSurface::BytesPerBlock; i++)
		{
			GetOutPayload()[1 + 1 + i] = SurfaceData[IndexOffset + i];
		}
	}

	void SendUpdateFinishedPacket()
	{
		SendMetaHashPacket(SYNC_META_SUB_HEADER_UPDATE_FINISHED);
	}

	bool SendBlockPacketHeader(const uint8_t index)
	{
		if (index < TrackedSurface->BlockCount)
		{
			GetOutPayload()[0] = index;
			GetOutPayload()[1] = GetLocalHash();
			FillBlockPacketData(index);

			RequestSendPacket(DataDefinition->Header);

			return true;
		}
		else
		{
			return false;
		}
	}
};
#endif
