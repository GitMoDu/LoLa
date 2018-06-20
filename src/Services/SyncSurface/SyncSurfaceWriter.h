// SyncSurfaceWriter.h

#ifndef _SYNCSURFACEWRITER_h
#define _SYNCSURFACEWRITER_h

#define LOLA_SYNC_SURFACE_SERVICE_SEND_NEXT_BLOCK_BACK_OFF_PERIOD_MILLIS	4
#define LOLA_SYNC_SURFACE_REMOTE_HASH_INVALIDATE_PERIOD_MILLIS				100000

#define SYNC_WRITER_MAX_ENSURE_TRY_COUNT 3

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
	enum SyncWriterState
	{
		Disabled,
		WaitingForSyncStart,
		SyncStart,
		SyncStarted,
		SendingStartStatus,
		WaitingForStartAck,
		UpdatingBlocks,
		WaitingForBlockSend,
		BlocksUpdated,
		BlocksDone,
		SendingFinishedStatus,
		WaitingForFinishedAck,
		PreparingForEnsure,
		WaitingForEnsure
	} WriterState = Disabled;

	uint8_t SurfaceSendingIndex = 0;


protected:
	uint32_t RemoteInvalidations = 0;

protected:
	virtual bool OnSetup()
	{
		if (SyncSurfaceBase::OnSetup())
		{
			WriterState = SyncWriterState::WaitingForSyncStart;
			return true;
		}

		return false;
	}

	void OnSurfaceDataChanged()
	{
		if (WriterState != SyncWriterState::Disabled && WriterState != SyncWriterState::WaitingForSyncStart)
		{
			Enable();
			SetNextRunASAP();
		}
	}

	void OnSendFailed()
	{
		TrackedSurface->GetTracker()->SetAllPending();
		InvalidateSync();
	}

	void OnSendTimedOut()
	{
		OnSendFailed();
	}

	bool OnSyncStart()
	{
		if (SyncSurfaceBase::OnSyncStart())
		{
			WriterState = WaitingForSyncStart;
			return true;
		}

		return false;
	}

	void OnSyncAchieved()
	{
		StampSessionSynced();
#ifdef DEBUG_LOLA
		Serial.println(F("Data in Sync"));
#endif		
	}

	void OnSyncLost()
	{
#ifdef DEBUG_LOLA
		Serial.println(F("Sync Lost"));
#endif		
		if (WriterState != SyncWriterState::Disabled)
		{
			StartSync();
		}
	}

	void PrepareNextPending()
	{
		if (SurfaceSendingIndex >= TrackedSurface->GetSize())
		{
			SurfaceSendingIndex = 0;
		}
		SurfaceSendingIndex = TrackedSurface->GetTracker()->GetNextPendingIndex(SurfaceSendingIndex);
		PrepareBlockPacket(SurfaceSendingIndex);
		PrepareBlockPayload(SurfaceSendingIndex, Packet->GetPayload());
		SurfaceSendingIndex++;
	}

	void StartSync()
	{
		WriterState = SyncWriterState::SyncStarted;
		SyncTryCount = 0;
		SetNextRunASAP();
	}

	void OnReaderStartSyncReceived()
	{
		if (WriterState != SyncWriterState::Disabled)
		{
			StartSync();
		}
	}

	void OnReaderSyncEnsureReceived()
	{
		if (WriterState == SyncWriterState::WaitingForEnsure || 
			WriterState == SyncWriterState::PreparingForEnsure)
		{
			if (IsRemoteHashValid())
			{
				PromoteToSynced();
			}
			else
			{
				StartSync();
			}
		}
	}

	void OnWriterStartingSyncAck()
	{
		if (WriterState == SyncWriterState::WaitingForStartAck ||
			WriterState == SyncWriterState::SendingStartStatus)
		{
			WriterState = SyncWriterState::UpdatingBlocks;
			SetNextRunASAP();
		}
	}

	void OnWriterAllDataUpdatedAck()
	{
		if (WriterState == SyncWriterState::WaitingForFinishedAck ||
			WriterState == SyncWriterState::SendingFinishedStatus)
		{
			WriterState = SyncWriterState::WaitingForEnsure;
			SetNextRunASAP();
		}
	}

	void OnReaderDisable()
	{
		WriterState = SyncWriterState::Disabled;
	}

	void OnSyncFailed()
	{
		WriterState = SyncWriterState::WaitingForSyncStart;
	}

	void OnSendOk()
	{
		LastSentMillis = Millis();
		SetNextRunASAP();
	}

	void OnSendDelayed()
	{
		if (WriterState == SyncWriterState::WaitingForBlockSend)
		{
			PrepareNextPending();//Update packet payload with latest live data.
		}
	}

	bool OnNotSynced()
	{
		switch (WriterState)
		{
		case SyncWriterState::Disabled:
			SetNextRunDefault();
			break;
		case SyncWriterState::WaitingForSyncStart:
			SetNextRunLong();
			break;
		case SyncWriterState::SyncStart:
			SyncStartMillis = Millis();
			break;
		case SyncWriterState::SyncStarted:
			if (Millis() - SyncStartMillis > LOLA_SYNC_SURFACE_MAX_UNRESPONSIVE_REMOTE_DURATION_MILLIS)
			{
				OnSyncFailed();
				SetNextRunLong();
			}
			else
			{
				WriterState = SyncWriterState::SendingStartStatus;
				SyncTryCount++;
				PrepareWriterStartingSyncPacket();
				RequestSendPacket();
			}
			break;
		case SyncWriterState::SendingStartStatus:
			WriterState = SyncWriterState::WaitingForStartAck;
			SetNextRunDelay(LOLA_SEND_SERVICE_REPLY_TIMEOUT_MILLIS);
			break;
		case SyncWriterState::WaitingForStartAck:
			//If we get here before progressing the state, it means sending has failed.
			if (SyncTryCount > SYNC_WRITER_MAX_ENSURE_TRY_COUNT)
			{
				WriterState = SyncWriterState::WaitingForSyncStart;
			}
			else
			{
				WriterState = SyncWriterState::SyncStarted;
			}
			SetNextRunASAP();
			break;
		case SyncWriterState::UpdatingBlocks:
			if (TrackedSurface->GetTracker()->HasPending())
			{
				PrepareNextPending();
				RequestSendPacket();
				WriterState = SyncWriterState::WaitingForBlockSend;
			}
			else
			{
				WriterState = SyncWriterState::BlocksUpdated;
				SetNextRunASAP();
			}
			break;
		case SyncWriterState::WaitingForBlockSend:
			TrackedSurface->GetTracker()->ClearBitPending(SurfaceSendingIndex);
			WriterState = SyncWriterState::UpdatingBlocks;
			//Elapsed time aware delay. Should keep things snappy without running over something.
			SetNextRunDelay(constrain(Millis() - LastSentMillis, 0, LOLA_SYNC_SURFACE_SERVICE_SEND_NEXT_BLOCK_BACK_OFF_PERIOD_MILLIS));
			break;
		case SyncWriterState::BlocksUpdated:
			WriterState = SyncWriterState::BlocksDone;
			if (SessionHasSynced())
			{
				//Wait a bit for potential block updates, before occupying comms with sync acknowledge.
				SetNextRunDelay(LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS);
			}
			else
			{
				//Except on first sync, let's get there fast.
				SetNextRunASAP();
			}
			break;
		case SyncWriterState::BlocksDone:
			PrepareWriterAllDataUpdated();
			RequestSendPacket();
			WriterState = SyncWriterState::SendingFinishedStatus;
			break;
		case SyncWriterState::SendingFinishedStatus:
			WriterState = SyncWriterState::WaitingForFinishedAck;
			SetNextRunDelay(LOLA_SEND_SERVICE_REPLY_TIMEOUT_MILLIS);
			break;
		case SyncWriterState::WaitingForFinishedAck:
			//If we get here before progressing the state, it means sending has failed.
			WriterState = SyncWriterState::SyncStarted;
			SetNextRunASAP();
		case SyncWriterState::PreparingForEnsure:
			WriterState = SyncWriterState::WaitingForEnsure;
			SetNextRunDelay(LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS);
			break;
		case SyncWriterState::WaitingForEnsure:
			WriterState = SyncWriterState::SyncStarted;
			SetNextRunASAP();
			break;
		}

		return false; //Always false, AbstractSyncStatus progresses through events.
	}

	uint32_t LastSentMillis;
	bool OnSynced()
	{
		if (TrackedSurface->GetTracker()->HasPending())
		{
			return true;
		}
		else if (!IsRemoteHashValid())
		{
			RemoteInvalidations++;
			return true;
		}
		else if (Millis() - LastReceived > LOLA_SYNC_SURFACE_MAX_UNRESPONSIVE_REMOTE_DURATION_MILLIS)
		{
			return true;
		}
		else if (Millis() - LastSentMillis > LOLA_SYNC_KEEP_ALIVE_MILLIS)
		{
			PrepareWriterAllDataUpdated();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDefault();
		}

		return false;
	}
};
#endif

