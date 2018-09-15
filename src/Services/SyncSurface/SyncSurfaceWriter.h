// SyncSurfaceWriter.h

#ifndef _SYNCSURFACEWRITER_h
#define _SYNCSURFACEWRITER_h


//#define LOLA_SYNC_SURFACE_SERVICE_SEND_NEXT_BLOCK_BACK_OFF_PERIOD_MILLIS	2

#include <Services\SyncSurface\SyncSurfaceBase.h>

class SyncSurfaceWriter : public SyncSurfaceBase
{
public:
	SyncSurfaceWriter(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedSurface* trackedSurface)
		: SyncSurfaceBase(scheduler, loLa, baseHeader, trackedSurface)
	{
	}

private:
	enum SyncWriterState : uint8_t
	{
		UpdatingBlocks = 0,
		SendingBlock = 1,
		SendingSyncComplete = 2,
	} WriterState = SyncWriterState::UpdatingBlocks;

	uint8_t SurfaceSendingIndex = 0;

protected:
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncSurfaceWriter"));

	}
#endif // DEBUG_LOLA

	void OnSendOk(const uint32_t sendDuration)
	{
		if (SyncState == SyncStateEnum::Syncing && WriterState == SyncWriterState::SendingBlock)
		{
			TrackedSurface->GetTracker()->ClearBit(SurfaceSendingIndex);
			SurfaceSendingIndex++;
		}
		SetNextRunASAP();
	}

	void OnSyncStateUpdated(const SyncStateEnum newState)
	{
		switch (newState)
		{
		case SyncStateEnum::WaitingForServiceDiscovery:
			Disable();
		case SyncStateEnum::Syncing:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			break;
		default:
			break;
		}
	}

	void OnServiceDiscoveryReceived()
	{
		if (SyncState != SyncStateEnum::Disabled)
		{
			UpdateSyncState(SyncStateEnum::Syncing);
		}
	}

	void OnWaitingForServiceDiscovery()
	{
		Disable();
	}

	void OnSyncResponseReceived()
	{
		if (SyncState == SyncStateEnum::Syncing)
		{
			switch (WriterState)
			{
			case SyncSurfaceWriter::UpdatingBlocks:
			case SyncSurfaceWriter::SendingBlock:
				if (HashesMatch())
				{
					PrepareFinishedSyncPacket();
					RequestSendPacket();
					UpdateSyncingState(SyncWriterState::SendingSyncComplete);
				}
				else
				{
					TrackedSurface->GetTracker()->SetAll();
				}
				break;
			case SyncSurfaceWriter::SendingSyncComplete:
				break;
			default:
				break;
			}
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
				if (GetElapsedSinceLastSent() > ABSTRACT_SURFACE_SYNC_SEND_BACK_OFF_PERIOD_MILLIS)
				{
					PrepareFinishingSyncPacket();
					RequestSendPacket();
				}
				else
				{
					SetNextRunDelay(ABSTRACT_SURFACE_SYNC_SEND_BACK_OFF_PERIOD_MILLIS);
				}
			}
			break;
		case SyncWriterState::SendingBlock:
			UpdateSyncingState(SyncWriterState::UpdatingBlocks);
			SetNextRunDelay(ABSTRACT_SURFACE_SYNC_SEND_BACK_OFF_PERIOD_MILLIS);
			break;
		case SyncWriterState::SendingSyncComplete:
			UpdateSyncState(SyncStateEnum::Synced);
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
			Serial.print(Millis());
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
				break;
			case SyncWriterState::SendingSyncComplete:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("SyncComplete"));
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
