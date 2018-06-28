// SyncSurfaceWriter.h

#ifndef _SYNCSURFACEWRITER_h
#define _SYNCSURFACEWRITER_h

#define LOLA_SYNC_SURFACE_SERVICE_SEND_NEXT_BLOCK_BACK_OFF_PERIOD_MILLIS	4
#define LOLA_SYNC_SURFACE_SERVICE_SEND_FINALIZER_BACK_OFF_PERIOD_MILLIS	    ABSTRACT_SURFACE_SYNC_REPLY_TIMEOUT

#define SYNC_WRITER_MAX_FINALIZE_TRY_COUNT 3

#include <Services\SyncSurface\SyncSurfaceBase.h>
#include <BitTracker.h>

class SyncSurfaceWriter : public SyncSurfaceBase
{
public:
	SyncSurfaceWriter(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedSurfaceNotify* trackedSurface)
		: SyncSurfaceBase(scheduler, loLa, baseHeader, trackedSurface)
	{
	}

private:
	enum SyncWriterState : uint8_t
	{
		SyncStarting = 0,
		SendingStart = 1,
		UpdatingBlocks = 2,
		SendingBlock = 3,
		BlocksUpdated = 4,
		BlocksDone = 5,
		SendingFinish = 6,
		WaitingForConfirmation = 7,
		SyncComplete = 8,
	} WriterState = SyncStarting;

	uint8_t SurfaceSendingIndex = 0;

protected:
	void OnSurfaceDataChanged()
	{
		InvalidateLocalHash();

		switch (SyncState)
		{
		case SyncStateEnum::Starting:
		case SyncStateEnum::FullSync:
		case SyncStateEnum::WaitingForTrigger:
		case SyncStateEnum::Resync:
		case SyncStateEnum::Synced:
			UpdateState(SyncStateEnum::Resync);
			break;
		case SyncStateEnum::Disabled:
		default:
			break;
		}
	}

	void OnSendOk(const uint32_t sendDuration)
	{
		if (IsSyncing() && WriterState == SyncWriterState::SendingBlock)
		{
			TrackedSurface->GetTracker()->ClearBitPending(SurfaceSendingIndex);
			SurfaceSendingIndex++;
		}
		SetNextRunASAP();
	}

	void OnStateUpdated(const SyncStateEnum newState)
	{
		SyncSurfaceBase::OnStateUpdated(newState);
		switch (newState)
		{
		case SyncStateEnum::Starting:
		case SyncStateEnum::FullSync:
			if (SyncState == SyncStateEnum::Resync)
			{
				TrackedSurface->GetTracker()->SetAllPending();
			}
			else
			{
				UpdateSyncingState(SyncWriterState::SyncStarting);
			}
			break;
		case SyncStateEnum::Resync:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			break;
		default:
			break;
		}
	}

	void OnWaitingForTriggerService()
	{
		//TODO: Replace with Disable, after testing.
		SetNextRunLong();//Writer does nothing until it receives a sync start request.
	}

	void OnSyncActive()
	{
		switch (WriterState)
		{
		case SyncWriterState::SyncStarting:
			SyncTryCount = 0;
			UpdateSyncingState(SyncWriterState::SendingStart);
			PrepareStartingSyncPacket();
			RequestSendPacket();
			break;
		case SyncWriterState::SendingStart:
			//If we get here before progressing the state, it means sending has failed.
			UpdateSyncingState(SyncWriterState::SyncStarting);
			SetNextRunDelay(LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS);
			break;
		case SyncWriterState::UpdatingBlocks:
			if (TrackedSurface->GetTracker()->HasPending())
			{
				PrepareNextPendingBlockPacket();
				RequestSendPacket();
				UpdateSyncingState(SyncWriterState::SendingBlock);
			}
			else
			{
				UpdateSyncingState(SyncWriterState::BlocksUpdated);
				//Take some time for the data to maybe update, before occupying the link with insurace.
				SetNextRunDelay(LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS);
			}
			break;
		case SyncWriterState::SendingBlock:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			SetNextRunDelay(LOLA_SYNC_SURFACE_SERVICE_SEND_NEXT_BLOCK_BACK_OFF_PERIOD_MILLIS);
			break;
		case SyncWriterState::BlocksUpdated:
			PrepareFinalizingProtocolPacket();
			RequestSendPacket();
			UpdateSyncingState(SyncWriterState::SendingFinish);
			SetNextRunDelay(LOLA_SYNC_SURFACE_SERVICE_SEND_FINALIZER_BACK_OFF_PERIOD_MILLIS);
			break;
		case SyncWriterState::SendingFinish:
			//If we're were, something went wrong with communicating the finishing move.
			SyncTryCount++;
			if (TrackedSurface->GetTracker()->HasPending())
			{
				UpdateSyncingState(SyncWriterState::UpdatingBlocks);
				SetNextRunASAP();
			}
			else if (SyncTryCount > SYNC_WRITER_MAX_FINALIZE_TRY_COUNT)
			{
				TrackedSurface->GetTracker()->SetAllPending();
				UpdateSyncingState(SyncWriterState::SyncStarting);
				SetNextRunASAP();
			}
			else
			{
				//Retry last block.
				//TODO: Improve this behaviour. We have few tries and unreliable memory of which blocks were sent on this session.
				SetLastSentBlockAsPending();
				UpdateSyncingState(SyncWriterState::UpdatingBlocks);
				SetNextRunASAP();
			}
			break;
		case SyncWriterState::WaitingForConfirmation:
			if (GetSubStateElapsed() > ABSTRACT_SURFACE_SYNC_REPLY_TIMEOUT)
			{
#ifdef DEBUG_LOLA
				Serial.print(Millis());
				Serial.println(F(": WaitingForConfirmation time out"));
#endif
				//If we're here, we've timed oud.
				SetLastSentBlockAsPending();
				UpdateSyncingState(SyncWriterState::UpdatingBlocks);
				SetNextRunASAP();
			}
			else
			{
				SetNextRunDelay(ABSTRACT_SURFACE_SYNC_REPLY_CHECK_PERIOD);
			}
			break;
		case SyncWriterState::SyncComplete:
			UpdateState(SyncStateEnum::Synced);
			break;
		default:
			UpdateState(SyncStateEnum::WaitingForTrigger);
			break;
		}
	}

	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		if (IsSyncing())
		{
			switch (WriterState)
			{
			case SyncWriterState::SyncStarting:
			case SyncWriterState::SendingStart:
				//Protocol sync has started.
				UpdateSyncingState(SyncWriterState::UpdatingBlocks);
				SetNextRunASAP();
				break;
			case SyncWriterState::SendingFinish:
				if (TrackedSurface->GetTracker()->HasPending())
				{
					UpdateSyncingState(SyncWriterState::UpdatingBlocks);
					SetNextRunASAP();
				}
				else
				{
					UpdateSyncingState(SyncWriterState::WaitingForConfirmation);
				}
				break;
			default:
				break;
			}
		}
	}

	void OnSyncStartRequestReceived()
	{
#ifdef DEBUG_LOLA
		Serial.print(Millis());
		Serial.println(F(": SyncStartRequest Received"));
#endif
		switch (SyncState)
		{
		case SyncStateEnum::Synced:
		case SyncStateEnum::FullSync:
		case SyncStateEnum::Starting:
		case SyncStateEnum::WaitingForTrigger:
		case SyncStateEnum::Resync:
			TrackedSurface->GetTracker()->SetAllPending();
			UpdateState(SyncStateEnum::FullSync);
			break;
		default:
			break;
		}
	}

	void OnDoubleCheckReceived()
	{
		switch (SyncState)
		{
		case SyncStateEnum::Starting:
			break;
		case SyncStateEnum::WaitingForTrigger:
		case SyncStateEnum::FullSync:
		case SyncStateEnum::Resync:
			TrackedSurface->GetTracker()->SetAllPending();
			UpdateState(SyncStateEnum::FullSync);
			break;
		case SyncStateEnum::Synced:
			if (!HashesMatch())
			{
				UpdateState(SyncStateEnum::Starting);
			}
			else if (TrackedSurface->GetTracker()->HasPending())
			{
				UpdateState(SyncStateEnum::Resync);
				SetNextRunASAP();
			}
			break;
		case SyncStateEnum::Disabled:
			break;
		default:
			break;
		}
	}

	void OnSyncReportReceived(uint8_t* payload)
	{
#ifdef DEBUG_LOLA
		Serial.print(Millis());
		Serial.println(F(": SyncReport Received"));
#endif
		switch (SyncState)
		{
		case SyncStateEnum::FullSync:
		case SyncStateEnum::Resync:
			switch (WriterState)
			{
			case SyncSurfaceWriter::BlocksDone:
			case SyncSurfaceWriter::SendingFinish:
			case SyncSurfaceWriter::WaitingForConfirmation:
			case SyncSurfaceWriter::SyncComplete:
				if (!LocalHashNeedsUpdate && HashesMatch())
				{
					if (TrackedSurface->GetTracker()->HasPending())
					{
						UpdateSyncingState(SyncWriterState::UpdatingBlocks);
					}
					else
					{
						UpdateSyncingState(SyncWriterState::SyncComplete);
					}
				}
				SetNextRunASAP();
				break;			
			default:
				UpdateSyncingState(SyncWriterState::SyncStarting);
				break;
			}
			break;
		default:
			UpdateSyncingState(SyncWriterState::SyncStarting);
			break;
		}
	}

	void OnSyncDisableRequestReceived()
	{
		UpdateState(SyncStateEnum::WaitingForTrigger);
	}

	void OnSendTimedOut()
	{
		OnSendPacketFailed();
	}

	void OnSendFailed()
	{
		OnSendPacketFailed();
	}

	void OnSendDelayed()
	{
		if (IsSyncing() && WriterState == SyncWriterState::SendingBlock)
		{
			UpdatePendingBlockPacketPayload();//Update packet payload with latest live data.
		}
	}

	void OnSendRetrying()
	{
		if (IsSyncing() && WriterState == SyncWriterState::SendingBlock)
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
			StampSubStateStart();

			switch (WriterState)
			{
			default:
				break;
			}
#ifdef DEBUG_LOLA
			Serial.print(Millis());
			Serial.print(F(": Updated Writer Syncing to "));
#endif
			switch (newState)
			{
			case SyncSurfaceWriter::SyncStarting:
#ifdef DEBUG_LOLA
				Serial.println(F("SyncStarting"));
#endif
				break;
			case SyncSurfaceWriter::SendingStart:
#ifdef DEBUG_LOLA
				Serial.println(F("SendingStart"));
#endif
				break;
			case SyncSurfaceWriter::UpdatingBlocks:
#ifdef DEBUG_LOLA
				Serial.println(F("UpdatingBlocks"));
#endif
				break;
			case SyncSurfaceWriter::SendingBlock:
#ifdef DEBUG_LOLA
				Serial.println(F("SendingBlock"));
#endif
				break;
			case SyncSurfaceWriter::BlocksUpdated:
#ifdef DEBUG_LOLA
				Serial.println(F("BlocksUpdated"));
#endif
				break;
			case SyncSurfaceWriter::BlocksDone:
#ifdef DEBUG_LOLA
				Serial.println(F("BlocksDone"));
#endif
				break;
			case SyncSurfaceWriter::SendingFinish:
#ifdef DEBUG_LOLA
				Serial.println(F("SendingFinish"));
#endif
				break;
			case SyncSurfaceWriter::WaitingForConfirmation:
#ifdef DEBUG_LOLA
				Serial.println(F("WaitingForConfirmation"));
#endif
				SetNextRunDelay(ABSTRACT_SURFACE_SYNC_REPLY_TIMEOUT);
				break;
			case SyncSurfaceWriter::SyncComplete:
#ifdef DEBUG_LOLA
				Serial.println(F("SyncComplete"));
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
		PrepareBlockPacketPayload(SurfaceSendingIndex, Packet->GetPayload());
	}

	void PrepareFinalizingProtocolPacket()
	{
		PrepareProtocolPacket(SYNC_SURFACE_PROTOCOL_SUB_HEADER_FINISHING_SYNC);
	}

	void PrepareNextPendingBlockPacket()
	{
		if (SurfaceSendingIndex >= TrackedSurface->GetSize())
		{
			SurfaceSendingIndex = 0;
		}
		SurfaceSendingIndex = TrackedSurface->GetTracker()->GetNextPendingIndex(SurfaceSendingIndex);
		PrepareBlockPacketHeader(SurfaceSendingIndex);
		PrepareBlockPacketPayload(SurfaceSendingIndex, Packet->GetPayload());
	}

	void PrepareStartingSyncPacket()
	{
		PrepareProtocolPacket(SYNC_SURFACE_PROTOCOL_SUB_HEADER_STARTING_SYNC);
		Packet->GetPayload()[0] = GetLocalHash();
	}

	void SetLastSentBlockAsPending()
	{
		if (SurfaceSendingIndex < TrackedSurface->GetSize())
		{
			TrackedSurface->GetTracker()->SetBitPending(SurfaceSendingIndex);
			if (SurfaceSendingIndex > 1)
			{
				SurfaceSendingIndex--;
			}
		}
		else
		{
			if (SurfaceSendingIndex > 0)
			{
				TrackedSurface->GetTracker()->SetBitPending(SurfaceSendingIndex - 1);
				if (SurfaceSendingIndex > 1)
				{
					SurfaceSendingIndex--;
				}
			}
		}
	}

	void OnSendPacketFailed()
	{
		//TODO: Should respond to packets.
		SetNextRunASAP();
		switch (SyncState)
		{
		case SyncStateEnum::FullSync:
		case SyncStateEnum::Resync:
			SetLastSentBlockAsPending();
			break;
		default:
			break;
		}
	}
};
#endif