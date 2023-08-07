// SyncSurfaceWriter.h

#ifndef _SYNC_SURFACE_WRITER_h
#define _SYNC_SURFACE_WRITER_h

#include "AbstractSyncSurface.h"
#include "SyncSurfaceDefinitions.h"

/// <summary>
/// TODO: On Pre-send, check if block has been updated, 
/// and update payload/ unflag block.
/// 
/// </summary>
template <const uint8_t Port,
	const uint32_t NoDiscoveryTimeOut = 30000>
class SyncSurfaceWriter
	: public AbstractSyncSurface<Port, SyncSurfaceMaxPayloadSize, NoDiscoveryTimeOut>
	, public virtual ISurfaceListener
{
private:
	using BaseClass = AbstractSyncSurface<Port, SyncSurfaceMaxPayloadSize, NoDiscoveryTimeOut>;

	using BaseClass::FAST_CHECK_PERIOD_MILLIS;
	using BaseClass::SLOW_CHECK_PERIOD_MILLIS;
	using BaseClass::UPDATE_BACK_OFF_PERIOD_MILLIS;
	using BaseClass::SYNC_CONFIRM_RESEND_PERIOD_MILLIS;

protected:
	using BaseClass::TrackedSurface;
	using BaseClass::SurfaceData;
	using BaseClass::IsSynced;
	using BaseClass::HashesMatch;
	using BaseClass::SetRemoteHash;
	using BaseClass::GetLocalHash;
	using BaseClass::InvalidateLocalHash;
	using BaseClass::InvalidateRemoteHash;
	using BaseClass::ForceUpdateLocalHash;
	using BaseClass::ResetPacketThrottle;
	using BaseClass::PacketThrottle;
	using BaseClass::RequestSendPacket;
	using BaseClass::UpdateSyncState;
	using BaseClass::RequestSendMetaPacket;
	using BaseClass::GetElapsedSinceLastSent;

	using BaseClass::OutPacket;

private:
	enum WriterStateEnum
	{
		Updating,
		Validating,
		Throttling
	} WriterState = WriterStateEnum::Updating;

	uint32_t LastThrottleStart = 0;
	uint32_t ThrottlePeriod = 50;

	uint8_t SurfaceSendingIndex = 0;

public:
	SyncSurfaceWriter(Scheduler& scheduler, ILoLaLink* link, ITrackedSurface& trackedSurface, uint32_t throttlePeriod = 50)
		: BaseClass(scheduler, link, trackedSurface)
		, ThrottlePeriod(throttlePeriod)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SurfaceWriter ("));
		TrackedSurface.PrintName(serial);
		serial->print(F(")"));
	}
#endif

public:
	virtual const bool Setup() final
	{
		if (BaseClass::Setup())
		{
			return TrackedSurface.AttachSurfaceListener(this);
		}

		return false;
	}

	virtual void OnSurfaceUpdated(const uint8_t surfaceId) final
	{
		InvalidateLocalHash();
		Task::enableIfNot();
		Task::forceNextIteration();
	}

protected:
	virtual void OnDiscoveredService() final
	{
		if (IsSynced())
		{
			if (HashesMatch() && !TrackedSurface.HasAnyBlockPending())
			{
				Task::disable();
			}
			else
			{
				UpdateSyncState(false);
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Abstract Sync Caught changes while synced."));
#endif	
			}
		}
		else
		{
			const uint32_t timestamp = millis();
			switch (WriterState)
			{
			case WriterStateEnum::Updating:
				OnUpdating();
				break;

			case WriterStateEnum::Validating:
				// State is transitioned to Throttling on packet receive.
				if (GetElapsedSinceLastSent() > SYNC_CONFIRM_RESEND_PERIOD_MILLIS &&
					RequestSendMetaPacket(WriterCheckHashDefinition::HEADER))
				{
					Task::enableIfNot();
				}
				else
				{
					Task::delay(FAST_CHECK_PERIOD_MILLIS);
				}
				break;
			case WriterStateEnum::Throttling:
				if (timestamp - LastThrottleStart > ThrottlePeriod)
				{
					LastThrottleStart = timestamp;
					if (!TrackedSurface.HasAnyBlockPending() && HashesMatch())
					{
						UpdateSyncState(true);
					}
					else
					{
						WriterState = WriterStateEnum::Updating;
						Task::enableIfNot();
					}
				}
				else
				{
					Task::delay(FAST_CHECK_PERIOD_MILLIS);
				}
				break;
			default:
				WriterState = WriterStateEnum::Updating;
				Task::enableIfNot();
				break;
			}
		}
	}

	virtual void OnDiscoveredPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize) final
	{
		switch (payload[WriterUpdateBlockDefinition::HEADER_INDEX])
		{
		case ReaderInvalidateDefinition::HEADER:
			if (payloadSize == ReaderInvalidateDefinition::PAYLOAD_SIZE)
			{
				SetRemoteHash(payload[WriterCheckHashDefinition::CRC_OFFSET]);
				InvalidateLocalHash();
				TrackedSurface.SetAllBlocksPending();
				Task::enableIfNot();
			}
			break;
		case ReaderValidateHashDefinition::HEADER:
			if (payloadSize == ReaderValidateHashDefinition::PAYLOAD_SIZE)
			{
				SetRemoteHash(payload[WriterCheckHashDefinition::CRC_OFFSET]);
				if (WriterState == WriterStateEnum::Validating)
				{
					WriterState = WriterStateEnum::Throttling;
				}
				Task::enableIfNot();
			}
			break;
		default:
			break;
		}
	}

	virtual void OnServiceStarted() final
	{
		TrackedSurface.SetAllBlocksPending();
		LastThrottleStart = millis() - ThrottlePeriod;
		BaseClass::OnServiceStarted();
	}

	virtual void OnStateUpdated(const bool newState) final
	{
		if (!newState)
		{
			WriterState = WriterStateEnum::Updating;
		}
		Task::enableIfNot();
	}

private:
	const bool RequestSendBlockPacket(const uint8_t index)
	{
		OutPacket.SetPort(Port);
		OutPacket.SetHeader(WriterUpdateBlockDefinition::HEADER);
		OutPacket.Payload[WriterUpdateBlockDefinition::CRC_OFFSET] = GetLocalHash();
		OutPacket.Payload[WriterUpdateBlockDefinition::INDEX_OFFSET] = index;

		return RequestSendPacket(WriterUpdateBlockDefinition::PAYLOAD_SIZE);
	}

	void OnUpdating()
	{
		if (TrackedSurface.HasAnyBlockPending())
		{
			const uint8_t nextIndex = TrackedSurface.GetNextBlockPendingIndex(SurfaceSendingIndex);

			if (nextIndex < SurfaceSendingIndex)
			{
				// Looping around, wait for throttling.
				WriterState = WriterStateEnum::Throttling;
				Task::delay(FAST_CHECK_PERIOD_MILLIS);
			}
			else
			{
				if (RequestSendBlockPacket(nextIndex))
				{
					SurfaceSendingIndex = nextIndex;
				}
				else
				{
					Task::delay(FAST_CHECK_PERIOD_MILLIS);
				}
			}
		}
		else if (HashesMatch())
		{
			WriterState = WriterStateEnum::Validating;
			Task::delay(FAST_CHECK_PERIOD_MILLIS);
		}
		else
		{
			Task::disable();
		}
	}
};
#endif
