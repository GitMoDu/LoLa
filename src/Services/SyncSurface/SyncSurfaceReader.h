// SyncSurfaceReader.h

#ifndef _SYNCSURFACEREADER_h
#define _SYNCSURFACEREADER_h

#include <Services\SyncSurface\SyncSurfaceBase.h>


#define SYNC_READER_MAX_SYNC_TRY_COUNT 3

class SyncSurfaceReader : public SyncSurfaceBase
{
public:
	SyncSurfaceReader(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedSurfaceNotify* trackedSurface)
		: SyncSurfaceBase(scheduler, loLa, baseHeader, trackedSurface)
	{
	}

private:
	enum SyncReaderState
	{
		SyncStart,
		RequestingSyncStart,
		SendingSyncStart,
		PreparingForWriterStart,
		WaitingForWriterStart,
		WaitingForSyncStatus,
		WaitingForDataUpdate,
		PreparingForEnsure,
		SendindEnsure,
		SyncingComplete,
		Disabled
	} ReaderState = Disabled;

protected:
	void OnSyncAchieved()
	{
#ifdef DEBUG_LOLA
		Serial.println(F("OnSyncAchieved"));
#endif
		SyncTryCount = 0;
	}

	void OnSyncLost()
	{
#ifdef DEBUG_LOLA
		Serial.println(F("OnSyncLost"));
#endif		
		SyncTryCount = 0;
	}

	virtual bool OnSetup()
	{
		if (SyncSurfaceBase::OnSetup())
		{
			ReaderState = SyncReaderState::SyncStart;
			return true;
		}

		ReaderState = SyncReaderState::Disabled;

		return false;
	}

	void OnSendOk()
	{
		SetNextRunASAP();
	}

	void OnSendFailed()
	{

	}

	void OnSendTimedOut()
	{
		OnSendFailed();
	}

	bool OnSyncStart()
	{
		if (SyncSurfaceBase::OnSyncStart())
		{
			return true;
		}

		return false;
	}

	void OnBlockReceived(const uint8_t index, uint8_t * payload)
	{
		UpdateBlockData(index, payload);
		InvalidateHash();
		if (IsSynced())
		{
			InvalidateSync();
			NotifyDataChanged();
		}
		else
		{
			if (ReaderState == 1)
			{

			}
			TrackedSurface->GetTracker()->ClearBitPending(index);
			SetNextRunASAP();
		}
	}

	void OnReaderStartSyncAck()
	{
		if (IsSynced())
		{
			InvalidateSync();
		}
		else
		{
			if (ReaderState == SyncReaderState::SendingSyncStart)
			{
				ReaderState = SyncReaderState::PreparingForWriterStart;
				SetNextRunASAP();
			}
		}
	}

	void OnWriterStartingSyncReceived()
	{
		if (IsSynced())
		{
			InvalidateSync();
		}
		else if (ReaderState == SyncReaderState::PreparingForWriterStart ||
			ReaderState == SyncReaderState::WaitingForWriterStart)
		{
			ReaderState = SyncReaderState::SyncStart;
			SetNextRunASAP();
		}
	}

	void OnReaderSyncEnsureAck()
	{
		if (IsSynced())
		{
			InvalidateSync();
		}
		else if (ReaderState == SyncReaderState::SendindEnsure)
		{
			ReaderState = SyncReaderState::SyncingComplete;
			SetNextRunASAP();
		}
	}

	void OnWriterAllDataUpdatedReceived()
	{
		if (IsSynced() || ReaderState == SyncReaderState::WaitingForDataUpdate)
		{
			if (IsRemoteHashValid())
			{
				ReaderState = SyncReaderState::PreparingForEnsure;
			}
			else if (SessionHasSynced())
			{
				ReaderState = SyncReaderState::PreparingForEnsure;
				//TODO: Missed some bits, how to handle on Writer?
			}
			else
			{
				ReaderState = SyncReaderState::SyncStart;
			}
			SetNextRunASAP();
		}
		else if (ReaderState != SyncReaderState::Disabled)
		{
			//Protocol miss-hit, this edge case shouldn't happen often.
			ReaderState = SyncReaderState::SyncStart;
			SetNextRunASAP();
		}
	}

	bool OnNotSynced()
	{
		switch (ReaderState)
		{
		case SyncReaderState::SyncStart:
			SyncTryCount = 0;
			ReaderState = SyncReaderState::RequestingSyncStart;
			SetNextRunASAP();
			break;
		case SyncReaderState::RequestingSyncStart:
			ReaderState = SyncReaderState::SendingSyncStart;
			ResetSyncSession();
			PrepareReaderStartSyncPacket();
			RequestSendPacket();
			SyncStartMillis = Millis();
			break;
		case SyncReaderState::SendingSyncStart:
			//If we get here before progressing the state, it means sending has failed.
			ReaderState = SyncReaderState::RequestingSyncStart;
			SetNextRunDelay(LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS);
			break;
		case SyncReaderState::PreparingForWriterStart:
			ReaderState = SyncReaderState::WaitingForWriterStart;
			SetNextRunDelay(LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS);
			break;
		case SyncReaderState::WaitingForWriterStart:
			//If we get here before progressing the state, it means sending has failed.
			ReaderState = SyncReaderState::RequestingSyncStart;
			break;
		case SyncReaderState::WaitingForSyncStatus:
			//Sync-In-Progress barrier crossed.
			ReaderState = SyncReaderState::WaitingForDataUpdate;
			SetNextRunASAP();
			break;
		case SyncReaderState::WaitingForDataUpdate:
			if (LastReceived == ILOLA_INVALID_MILLIS || 
				LastReceived - GetLoLa()->GetMillis() > LOLA_SYNC_SURFACE_SERVICE_INVALIDATE_PERIOD_MILLIS)
			{
				if (SyncTryCount > SYNC_READER_MAX_SYNC_TRY_COUNT)
				{
					ReaderState = SyncReaderState::Disabled;
					OnSyncFailed();
					SyncTryCount = 0;
				}
				SyncTryCount++;
				SetNextRunASAP();
			}
			else
			{
				SetNextRunDefault();
			}
			break;
		case SyncReaderState::PreparingForEnsure:
			PrepareReaderEnsureSyncPacket();
			RequestSendPacket();
			ReaderState = SyncReaderState::SendindEnsure;
			break;
		case SyncReaderState::SendindEnsure:
			//If we get here before progressing the state, it means sending has failed.
			ReaderState = SyncReaderState::RequestingSyncStart;
			SetNextRunASAP();
			break;
		case SyncReaderState::SyncingComplete:
			if (IsRemoteHashValid())
			{
				return true;
			}
			else
			{
				//Partial data corruption, try again.
				ReaderState = SyncReaderState::RequestingSyncStart;
				SetNextRunASAP();
			}
			break;
		default:
		case SyncReaderState::Disabled:
			SetNextRunLong();
			break;
		}

		return false;
	}

	bool OnSynced()
	{
		if (IsRemoteHashValid())
		{
			return true;
		}
		else if (LastReceived == ILOLA_INVALID_MILLIS || 
			Millis() - LastReceived > LOLA_SYNC_SURFACE_MAX_UNRESPONSIVE_REMOTE_DURATION_MILLIS)
		{
			return true;
		}
		else if (Millis() - LastReceived > LOLA_SYNC_SURFACE_SERVICE_INVALIDATE_PERIOD_MILLIS)
		{
			if (SyncTryCount > SYNC_READER_MAX_SYNC_TRY_COUNT)
			{
				ReaderState = SyncReaderState::Disabled;
				OnSyncFailed();
			}
			else
			{
				SyncTryCount++;
			}
			SetNextRunASAP();
		}
		else
		{
			SetNextRunDefault();
		}

		return false;
	}
};

#endif