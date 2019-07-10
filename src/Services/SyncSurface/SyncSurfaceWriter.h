// SyncSurfaceWriter.h

#ifndef _SYNCSURFACEWRITER_h
#define _SYNCSURFACEWRITER_h


#include <Services\SyncSurface\SyncSurfaceBase.h>

template <const uint8_t BasePacketHeader>
class SyncSurfaceWriter : public SyncSurfaceBase
{
private:
	SyncMetaPacketDefinition<BasePacketHeader> SyncMetaDefinition;
	SyncDataPacketDefinition<BasePacketHeader> DataPacketDefinition;

public:
	SyncSurfaceWriter(Scheduler* scheduler, ILoLaDriver* driver, ITrackedSurface* trackedSurface, const bool autoStart = true)
		: SyncSurfaceBase(scheduler, driver, trackedSurface, &SyncMetaDefinition, &DataPacketDefinition, autoStart)
	{
	}

private:
	enum SyncWriterState : uint8_t
	{
		UpdatingBlocks = 0,
		SendingBlock = 1,
		SendingFinished = 2
	} WriterState = SyncWriterState::UpdatingBlocks;

	uint8_t SurfaceSendingIndex = 0;

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncSurfaceWriter"));

	}
#endif // DEBUG_LOLA

	void OnSendOk(const uint8_t header, const uint32_t sendDuration)
	{
		if (SyncState == SyncStateEnum::Syncing && WriterState == SyncWriterState::SendingBlock)
		{
			TrackedSurface->GetTracker()->ClearBit(SurfaceSendingIndex);
			SurfaceSendingIndex++;
		}
		SetNextRunASAP();
	}

	void OnStateUpdated(const SyncStateEnum newState)
	{
		switch (newState)
		{
		case SyncStateEnum::WaitingForServiceDiscovery:
			InvalidateRemoteHash();
			Disable();
		case SyncStateEnum::Syncing:
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
		case SyncStateEnum::Syncing:
			break;
		case SyncStateEnum::Disabled:
			break;
		default:
			UpdateSyncState(SyncStateEnum::Syncing);
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
		case SyncStateEnum::WaitingForServiceDiscovery:
			break;
		case SyncStateEnum::Syncing:
			switch (WriterState)
			{
			case SyncWriterState::SendingFinished:
				if (TrackedSurface->GetTracker()->HasSet())
				{
					UpdateSyncingState(SyncWriterState::UpdatingBlocks);
				}
				else if (HashesMatch())
				{
					UpdateSyncState(SyncStateEnum::Synced);
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
		case SyncStateEnum::Synced:
			if (!HashesMatch())
			{
				TrackedSurface->GetTracker()->SetAll();
				UpdateSyncState(SyncStateEnum::Syncing);
			}
			break;
		case SyncStateEnum::Disabled:
			break;
		default:
			break;
		}
	}

	void OnInvalidateRequestReceived()
	{
		switch (SyncState)
		{
		case SyncStateEnum::WaitingForServiceDiscovery:
			break;
		case SyncStateEnum::Syncing:
			TrackedSurface->GetTracker()->SetAll();
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			break;
		case SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->SetAll();
			UpdateSyncState(SyncStateEnum::Syncing);
			break;
		case SyncStateEnum::Disabled:
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
				PrepareNextPendingBlockPacket();
				RequestSendPacket();
				UpdateSyncingState(SyncWriterState::SendingBlock);
			}
			else
			{
				UpdateSyncingState(SyncWriterState::SendingFinished);
				ResetLastSentTimeStamp();
				SetNextRunDelay(ABSTRACT_SURFACE_SYNC_CONFIRM_SEND_PERIOD_MILLIS);
			}
			break;
		case SyncWriterState::SendingBlock:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			SetNextRunDelay(ABSTRACT_SURFACE_UPDATE_BACK_OFF_PERIOD_MILLIS);
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
		if (SyncState == SyncStateEnum::Syncing && WriterState == SyncWriterState::SendingBlock)
		{
			UpdatePendingBlockPacketPayload();//Update packet payload with latest live data.
		}
	}

	void OnSendRetrying()
	{
		if (SyncState == SyncStateEnum::Syncing && WriterState == SyncWriterState::SendingBlock)
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
