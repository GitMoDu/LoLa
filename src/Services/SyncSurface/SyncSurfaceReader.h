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

private:
	enum SyncReaderState : uint8_t
	{
		SyncStarting = 0,
		WaitingForWriterStart = 1,
		WaitingForDataUpdate = 2,
		PreparingForReport = 3,
		SendindReport = 4,
		SyncingComplete = 5
	} ReaderState = SyncStarting;

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
		case SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->SetAll();
			TrackedSurface->GetTracker()->ClearBit(index);
			UpdateState(SyncStateEnum::Resync);
			break;
		case SyncStateEnum::FullSync:
		case SyncStateEnum::Resync:
			switch (ReaderState)
			{
			case SyncReaderState::SyncStarting:
			case SyncReaderState::WaitingForWriterStart:
				UpdateSyncingState(SyncReaderState::WaitingForDataUpdate);
				break;
			case SyncReaderState::WaitingForDataUpdate:
				//Reset the time out by prolonging it to ABSTRACT_SURFACE_MAX_ELAPSED_BEFORE_SYNC_RESTART from now
				StampSubStateStart();
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
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
				UpdateSyncingState(SyncReaderState::SyncStarting);
			}
			break;
		case SyncStateEnum::Resync:
			UpdateSyncingState(SyncReaderState::WaitingForDataUpdate);
			break;
		default:
			break;
		}
	}

	void OnWaitingForTriggerService()
	{
		UpdateState(SyncStateEnum::FullSync);
	}

	void OnSyncActive()
	{
		switch (ReaderState)
		{
		case SyncReaderState::SyncStarting:
			UpdateSyncingState(SyncReaderState::WaitingForWriterStart);
			break;
		case SyncReaderState::WaitingForWriterStart:
			if (GetSubStateElapsed() > LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS)
			{
				StampSubStateStart();
				PrepareSyncStartRequestPacket();
				RequestSendPacket();
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.print(Millis());
				Serial.println(F(": Start sync request."));
#endif
			}
			SetNextRunDelay(LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS);
			break;
		case SyncReaderState::WaitingForDataUpdate:
			if (GetSubStateElapsed() > ABSTRACT_SURFACE_MAX_ELAPSED_BEFORE_SYNC_RESTART)
			{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.print(Millis());
				Serial.print(F(": WaitingForDataUpdate Timeout. Elapsed: "));
				Serial.print(GetSubStateElapsed());
#endif
				UpdateSyncingState(SyncReaderState::SyncStarting);
			}
			else
			{
				SetNextRunDelay(ABSTRACT_SURFACE_SYNC_REPLY_CHECK_PERIOD);
			}
			break;
		case SyncReaderState::PreparingForReport:
			UpdateLocalHash();
			PrepareSyncFinishedPacket();
			RequestSendPacket();
			UpdateSyncingState(SyncReaderState::SendindReport);
			break;
		case SyncReaderState::SendindReport:
			SyncTryCount++;
			if (SyncTryCount > ABSTRACT_SURFACE_SYNC_PERSISTANCE_COUNT)
			{
				UpdateSyncingState(SyncReaderState::SyncStarting);
			}
			else
			{
				UpdateSyncingState(SyncReaderState::PreparingForReport, false);
				SetNextRunDelay(ABSTRACT_SURFACE_SYNC_RETRY_PERIDO);
			}
			break;
		case SyncReaderState::SyncingComplete:
			UpdateLocalHash();
			if (HasRemoteHash() && HashesMatch())
			{
				UpdateState(SyncStateEnum::Synced);
			}
			else
			{
				UpdateSyncingState(SyncReaderState::SyncStarting);
			}				
			break;
		default:
			UpdateState(SyncStateEnum::Starting);
			break;
		}
	}

	void OnAckReceived(const uint8_t header, const uint8_t id)
	{
		if (IsSyncing())
		{
			switch (ReaderState)
			{
			case SyncReaderState::SyncStarting:
				UpdateSyncingState(SyncReaderState::WaitingForWriterStart);
			case SyncReaderState::WaitingForWriterStart:
				SetNextRunDelay(LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS);
				break;
			case SyncReaderState::PreparingForReport:
			case SyncReaderState::SendindReport:
				UpdateSyncingState(SyncReaderState::SyncingComplete);
				break;
			default:
				break;
			}
		}
	}

	void OnSyncFinishingReceived()
	{
		if (IsSyncing())
		{
			switch (ReaderState)
			{
			case SyncReaderState::WaitingForDataUpdate:
				UpdateSyncingState(SyncReaderState::PreparingForReport);			
				break;
			default:
				break;
			}
		}
	}

	void OnSyncStartingReceived()
	{
		if (IsSyncing())
		{
			switch (ReaderState)
			{
			case SyncReaderState::WaitingForDataUpdate:
				StampSubStateStart();//If we're already awaiting data, delay the time out.
			case SyncReaderState::SyncStarting:
			case SyncReaderState::WaitingForWriterStart:
				UpdateSyncingState(SyncReaderState::WaitingForDataUpdate);
				break;
			default:
				break;
			}
		}
	}

private:
	void UpdateSyncingState(const SyncReaderState newState, const bool resetTryCount = true)
	{
		if (ReaderState != newState)
		{
			SetNextRunASAP();
			StampSubStateStart();
			if (resetTryCount)
			{
				SyncTryCount = 0;
			}

#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.print(Millis());
			Serial.print(F(": Updated Writer Syncing to "));
#endif
			switch (newState)
			{
			case SyncReaderState::SyncStarting:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("SyncStarting"));
#endif
				InvalidateRemoteHash();
				TrackedSurface->GetTracker()->SetAll();
				StampSubStateStart(-LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS * 2);
				break;
			case SyncReaderState::WaitingForWriterStart:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("WaitingForWriterStart"));
#endif
				SetNextRunDelay(LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS);
				break;
			case SyncReaderState::WaitingForDataUpdate:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("WaitingForDataUpdate"));
#endif
				break;
			case SyncReaderState::PreparingForReport:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("PreparingForReport"));
#endif
				InvalidateLocalHash();
				break;
			case SyncReaderState::SendindReport:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("SendindReport"));
#endif
				break;
			case SyncReaderState::SyncingComplete:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("SyncingComplete"));
#endif
				break;
			default:
				break;
			}

			ReaderState = newState;
		}
	}

	void PrepareSyncStartRequestPacket()
	{
		PrepareProtocolPacket(SYNC_SURFACE_PROTOCOL_SUB_HEADER_REQUEST_SYNC);
	}

	void PrepareSyncFinishedPacket()
	{
		PrepareReportPacketHeader();
		PrepareTrackerStatusPayload();
	}
};
#endif