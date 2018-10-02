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
			UpdateSyncingState(SyncReaderState::WaitingForDataUpdate);
			break;
		default:
			break;
		}
	}

	void OnSyncStateUpdated(const SyncStateEnum newState)
	{
		switch (newState)
		{
		case SyncStateEnum::Syncing:
			UpdateSyncingState(SyncReaderState::WaitingForDataUpdate);
			break;
		case SyncStateEnum::Synced:
			TrackedSurface->GetTracker()->ClearAll();
			break;
		default:
			break;
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
				UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
			}
			else
			{
				SetNextRunDelay(ABSTRACT_SURFACE_SYNC_REPLY_CHECK_PERIOD);
			}
			break;
		case SyncReaderState::WaitingForSyncComplete:
			PrepareUpdateFinishedReplyPacket();
			RequestSendPacket();
			//We assume we are synced as soon as the packet gets sent ok.
			break;
		default:
			UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
			break;
		}
	}

	void OnSendOk(const uint8_t header, const uint32_t sendDuration)
	{
		if (SyncState == SyncStateEnum::Syncing &&
			ReaderState == SyncReaderState::WaitingForSyncComplete &&
			HashesMatch())
		{
			UpdateSyncState(SyncStateEnum::Synced);
		}
		SetNextRunASAP();
	}

	void OnUpdateFinishedReceived()
	{
		switch (SyncState)
		{
		case SyncStateEnum::Syncing:
			switch (ReaderState)
			{
			case SyncReaderState::WaitingForDataUpdate:
				if (HashesMatch())
				{
					UpdateSyncingState(SyncReaderState::WaitingForSyncComplete);
				}
				//else 
				//{
				//	//TODO: Add resync sub-state, where the reader just asks the writer to start again.
				//	//UpdateSyncState(SyncStateEnum::WaitingForServiceDiscovery);
				//}
				break;
			default:
				break;
			}
			break;
		case SyncStateEnum::Synced:
			//TODO: Add resync sub-state, where the reader just asks the writer to start again.
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