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
		WaitingForDataUpdate = 0,
		WaitingForSyncComplete = 1,
	} ReaderState = WaitingForDataUpdate;

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
		case SyncStateEnum::WaitingForServiceDiscovery:
			UpdateSyncState(SyncStateEnum::Syncing);
			break;
		case SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->SetAll();
			TrackedSurface->GetTracker()->ClearBit(index);
			UpdateSyncState(SyncStateEnum::Syncing);
			break;
		case SyncStateEnum::Syncing:
			if (ReaderState == SyncReaderState::WaitingForSyncComplete)
			{
				UpdateSyncingState(SyncReaderState::WaitingForDataUpdate);
			}
			break;
		default:
			break;
		}
	}

	void OnSyncStateUpdated(const SyncStateEnum newState)
	{
		if (newState == SyncStateEnum::Syncing)
		{
			UpdateSyncingState(SyncReaderState::WaitingForDataUpdate);
		}
	}

	void OnWaitingForServiceDiscovery()
	{
		if (GetElapsedSinceLastSent() > ABSTRACT_SURFACE_SYNC_RETRY_PERIDO)
		{
			PrepareServiceDiscoveryPacket();
			RequestSendPacket();
		}
		else
		{
			SetNextRunDelay(ABSTRACT_SURFACE_SYNC_RETRY_PERIDO);
		}
	}

	void OnSyncActive()
	{
		switch (ReaderState)
		{
		case SyncReaderState::WaitingForDataUpdate:
			if (GetElapsedSinceStateStart() > ABSTRACT_SURFACE_MAX_ELAPSED_DATA_SYNC_LOST)
			{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.print(Millis());
				Serial.print(F(": WaitingForDataUpdate Timeout. Elapsed: "));
				Serial.print(GetElapsedSinceStateStart());
#endif
				UpdateSyncState(SyncStateEnum::Syncing);
			}
			else
			{
				SetNextRunDelay(ABSTRACT_SURFACE_SYNC_REPLY_CHECK_PERIOD);
			}
			break;
		case SyncReaderState::WaitingForSyncComplete:
			if (GetElapsedSinceLastSent() > ABSTRACT_SURFACE_SYNC_SEND_BACK_OFF_PERIOD_MILLIS)
			{
				PrepareFinishingResponsePacket();
				RequestSendPacket();
			}
			else
			{
				SetNextRunDelay(ABSTRACT_SURFACE_SYNC_RETRY_PERIDO);
			}
			break;
		default:
			UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
			break;
		}
	}

	void OnSyncFinishedReceived()
	{
		switch (SyncState)
		{
		case SyncStateEnum::Syncing:
			switch (ReaderState)
			{
			case SyncReaderState::WaitingForSyncComplete:
				if (HashesMatch())
				{
					UpdateSyncState(SyncStateEnum::Synced);
				}				
				break;
			default:
				break;
			}
			break;
		case SyncStateEnum::Synced:
			UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
			break;
		default:
			break;
		}
	}

	void OnSyncFinishingReceived()
	{
		switch (SyncState)
		{
		case SyncStateEnum::Syncing:
			switch (ReaderState)
			{
			case SyncReaderState::WaitingForDataUpdate:
				UpdateSyncingState(SyncReaderState::WaitingForSyncComplete);
				break;
			default:
				break;
			}
			break;
		case SyncStateEnum::Synced:
			UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
			break;
		default:
			break;
		}
	}

private:
	void UpdateSyncingState(const SyncReaderState newState)
	{
		if (ReaderState != newState)
		{
			SetNextRunASAP();

#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
			Serial.print(Millis());
			Serial.print(F(": Updated Reader to "));
#endif
			ReaderState = newState;

			switch (ReaderState)
			{
			case SyncReaderState::WaitingForDataUpdate:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("WaitingForDataUpdate"));
#endif
				InvalidateLocalHash();
				break;
			case SyncReaderState::WaitingForSyncComplete:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("WaitingForSyncComplete"));
#endif
				break;
			default:
				UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
				break;
			}
		}
	}
};
#endif