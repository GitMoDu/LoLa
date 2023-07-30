// SyncSurfaceReader.h

#ifndef _SYNC_SURFACE_READER_h
#define _SYNC_SURFACE_READER_h

#include "AbstractSyncSurface.h"

template <const uint8_t Port,
	const uint32_t NoDiscoveryTimeOut = 30000>
class SyncSurfaceReader : public AbstractSyncSurface<Port, SyncSurfaceMaxPayloadSize, NoDiscoveryTimeOut>
{
private:
	using BaseClass = AbstractSyncSurface<Port, SyncSurfaceMaxPayloadSize, NoDiscoveryTimeOut>;

	static const uint32_t RECEIVE_FAILED_PERIOD = 100;
	static const uint32_t RESEND_PERIOD_MILLIS = 30;

protected:
	using BaseClass::TrackedSurface;
	using BaseClass::SetRemoteHash;
	using BaseClass::SurfaceData;
	using BaseClass::IsSynced;
	using BaseClass::HashesMatch;
	using BaseClass::InvalidateLocalHash;
	using BaseClass::UpdateSyncState;
	using BaseClass::GetElapsedSinceLastSent;
	using BaseClass::FAST_CHECK_PERIOD_MILLIS;
	using BaseClass::SLOW_CHECK_PERIOD_MILLIS;
	using BaseClass::RequestSendMetaPacket;

private:
	uint32_t LastUpdateReceived = 0;
	bool UpdateRequested = false;

public:
	SyncSurfaceReader(Scheduler& scheduler, ILoLaLink* link, ITrackedSurface& trackedSurface)
		: BaseClass(scheduler, link, trackedSurface)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SurfaceReader ("));
		TrackedSurface.PrintName(serial);
		serial->print(F(")"));
	}
#endif

	virtual void OnDiscoveredPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize) final
	{
		switch (HeaderDefinition::HEADER_INDEX)
		{
		case WriterUpdateBlockDefinition::HEADER:
			if (payloadSize == WriterUpdateBlockDefinition::PAYLOAD_SIZE)
			{
				LastUpdateReceived = millis();
				SetRemoteHash(payload[WriterCheckHashDefinition::CRC_OFFSET]);
				OnBlockPacketReceived(payload);
				InvalidateLocalHash();
				Task::enable();
			}
			break;
		case WriterCheckHashDefinition::HEADER:
			if (payloadSize == WriterCheckHashDefinition::PAYLOAD_SIZE)
			{
				LastUpdateReceived = millis();
				SetRemoteHash(payload[WriterCheckHashDefinition::CRC_OFFSET]);
				InvalidateLocalHash();
				UpdateRequested = true;
				Task::enable();
			}
			break;
		default:
			break;
		}
	}

protected:
	virtual void OnDiscoveredService() final
	{
		if (UpdateRequested)
		{
			if (HashesMatch())
			{
				if (RequestSendMetaPacket(ReaderValidateHashDefinition::HEADER))
				{
					UpdateRequested = false;
					UpdateSyncState(true);
					Task::enableIfNot();
				}
				else
				{
					Task::delay(FAST_CHECK_PERIOD_MILLIS);
				}
			}
			else
			{
				if (RequestSendMetaPacket(ReaderInvalidateDefinition::HEADER))
				{
					UpdateRequested = false;
					Task::enableIfNot();
				}
				else
				{
					Task::delay(FAST_CHECK_PERIOD_MILLIS);
				}
			}
		}
		else if (IsSynced())
		{
			if (HashesMatch() || !TrackedSurface.HasAnyBlockPending())
			{
				Task::disable();
			}
			else
			{
				UpdateSyncState(false);
			}
		}
		else if (HashesMatch() || !TrackedSurface.HasAnyBlockPending())
		{
			if (GetElapsedSinceLastSent() > RESEND_PERIOD_MILLIS &&
				RequestSendMetaPacket(ReaderValidateHashDefinition::HEADER))
			{
				UpdateSyncState(true);
				Task::enableIfNot();
			}
			else
			{
				Task::delay(FAST_CHECK_PERIOD_MILLIS);
			}
		}
		else if (millis() - LastUpdateReceived > RECEIVE_FAILED_PERIOD)
		{
			if (GetElapsedSinceLastSent() > RESEND_PERIOD_MILLIS &&
				RequestSendMetaPacket(ReaderInvalidateDefinition::HEADER))
			{
				Task::enableIfNot();
			}
			else
			{
				Task::delay(FAST_CHECK_PERIOD_MILLIS);
			}
		}
		else
		{
			Task::delay(SLOW_CHECK_PERIOD_MILLIS);
		}
	}

	virtual void OnStateUpdated(const bool newState) final
	{
		Task::enableIfNot();
	}

	virtual void OnServiceStarted() final
	{
		TrackedSurface.SetAllBlocksPending();
		BaseClass::OnServiceStarted();
	}

private:
	void OnBlockPacketReceived(const uint8_t* payload)
	{
		const uint8_t index = payload[WriterUpdateBlockDefinition::INDEX_OFFSET];
		const uint8_t indexOffset = index * ITrackedSurface::BytesPerBlock;

		for (uint_least8_t i = 0; i < ITrackedSurface::BytesPerBlock; i++)
		{
			SurfaceData[indexOffset + i] = payload[WriterUpdateBlockDefinition::BLOCKS_OFFSET + i];
		}

		TrackedSurface.ClearBlockPending(index);
		TrackedSurface.NotifyUpdated();
	}
	/*void UpdateBlockData(const uint8_t* blockData, const uint8_t blockIndex)
	{

	}*/
};
#endif