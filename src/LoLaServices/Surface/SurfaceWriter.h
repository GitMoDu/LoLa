// SurfaceWriter.h

#ifndef _SURFACE_WRITER_h
#define _SURFACE_WRITER_h

#include "AbstractSurfaceService.h"


using namespace SurfaceDefinitions;

template <const uint8_t Port
	, const uint32_t ServiceId
	, const uint16_t ThrottlePeriodMillis = 50>
class SurfaceWriter
	: public AbstractSurfaceService<Port, ServiceId, SyncSurfaceMaxPayloadSize>
	, public virtual ISurfaceListener
{
private:
	using BaseClass = AbstractSurfaceService<Port, ServiceId, SyncSurfaceMaxPayloadSize>;

	using BaseClass::FAST_CHECK_PERIOD_MILLIS;

	enum class WriterStateEnum : uint8_t
	{
		Disabled,
		Updating,
		Throttling,
		Validating,
		Matching,
		Sleeping
	};

protected:
	using BaseClass::RequestSendPacket;
	using BaseClass::ResetPacketThrottle;
	using BaseClass::PacketThrottle;
	using BaseClass::OutPacket;

	using BaseClass::TrackedSurface;
	using BaseClass::SurfaceSize;
	using BaseClass::HashesMatch;
	using BaseClass::SetRemoteHashArray;
	using BaseClass::GetLocalHash;
	using BaseClass::RequestSendMeta;


private:
	uint32_t ThrottleStart = 0;
	uint16_t MatchHash = 0;

	uint8_t CurrentIndex = 0;

	WriterStateEnum WriterState = WriterStateEnum::Disabled;

public:
	SurfaceWriter(Scheduler& scheduler, ILoLaLink* link, ISurface* trackedSurface)
		: ISurfaceListener()
		, BaseClass(scheduler, link, trackedSurface)
	{}

public:
	virtual const bool Setup() final
	{
		if (BaseClass::Setup())
		{
			return TrackedSurface->AttachSurfaceListener(this);
		}

		return false;
	}

	virtual void OnServiceStarted() final
	{
		BaseClass::OnServiceStarted();
		if (SurfaceSize > 0)
		{
			WriterState = WriterStateEnum::Updating;
			CurrentIndex = 0;
			Task::enableDelayed(0);
			ThrottleStart = millis();
		}
	}

	virtual void OnServiceEnded() final
	{
		BaseClass::OnServiceEnded();
		WriterState = WriterStateEnum::Disabled;
#if defined(DEBUG_LOLA)
		Serial.println(F("Writer is Cold."));
#endif
	}

	virtual void OnSurfaceUpdated() final
	{
		if (SurfaceSize > 0
			&& WriterState == WriterStateEnum::Sleeping)
		{
			Task::enableDelayed(0);
		}
	}

protected:
	virtual void OnLinkedService() final
	{
		const uint32_t timestamp = millis();
		switch (WriterState)
		{
		case WriterStateEnum::Sleeping:
			if (HashesMatch())
			{
				TrackedSurface->ClearAllBlocksPending();
				CurrentIndex = 0;
				Task::disable();
			}
			else
			{
				WriterState = WriterStateEnum::Updating;
				Task::enableDelayed(0);
			}
			break;
		case WriterStateEnum::Updating:
			Task::delay(0);
			OnUpdating();
			break;
		case WriterStateEnum::Validating:
			if (PacketThrottle() &&
				RequestSendMeta(WriterCheckHashDefinition::HEADER))
			{
				if (!TrackedSurface->IsHot())
				{
					MatchHash = GetLocalHash();
				}
				Task::delay(0);
			}
			else
			{
				Task::delay(FAST_CHECK_PERIOD_MILLIS);
			}
			break;
		case WriterStateEnum::Matching:
			if (!TrackedSurface->IsHot())
			{
				if (HashesMatch(MatchHash))
				{
					TrackedSurface->SetHot(true);
#if defined(DEBUG_LOLA)
					Serial.println(F("Writer is Hot."));
#endif
				}
			}

			WriterState = WriterStateEnum::Throttling;
			Task::delay(0);
			if (!HashesMatch())
			{
				// Sync has been invalidated.
				TrackedSurface->SetAllBlocksPending();
			}
			break;
		case WriterStateEnum::Throttling:
			if (timestamp - ThrottleStart >= ThrottlePeriodMillis)
			{
				Task::delay(0);
				WriterState = WriterStateEnum::Sleeping;
				ThrottleStart = millis();
			}
			else
			{
				Task::delay(ThrottlePeriodMillis - (timestamp - ThrottleStart));
			}
			break;
		case WriterStateEnum::Disabled:
		default:
			Task::disable();
			break;
		}
	}

	virtual void OnLinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case ReaderValidateDefinition::HEADER:
			if (payloadSize == ReaderValidateDefinition::PAYLOAD_SIZE)
			{
				switch (WriterState)
				{
				case WriterStateEnum::Sleeping:
					Task::enableDelayed(0);
				case WriterStateEnum::Validating:
				case WriterStateEnum::Throttling:
					WriterState = WriterStateEnum::Matching;
					break;
				case WriterStateEnum::Matching:
				case WriterStateEnum::Updating:
					break;
				case WriterStateEnum::Disabled:
				default:
					return;
					break;
				}
				SetRemoteHashArray(&payload[ReaderValidateDefinition::CRC_OFFSET]);
			}
			break;
		case ReaderInvalidateDefinition::HEADER:
			if (payloadSize == ReaderInvalidateDefinition::PAYLOAD_SIZE)
			{
				switch (WriterState)
				{
				case WriterStateEnum::Sleeping:
					Task::enableDelayed(0);
				case WriterStateEnum::Updating:
					CurrentIndex = 0;
				case WriterStateEnum::Validating:
				case WriterStateEnum::Matching:
				case WriterStateEnum::Throttling:
					TrackedSurface->SetAllBlocksPending();
					break;
				case WriterStateEnum::Disabled:
				default:
					return;
					break;
				}
				SetRemoteHashArray(&payload[ReaderInvalidateDefinition::CRC_OFFSET]);
#if defined(DEBUG_LOLA)
				Serial.print(F("Writer Invalidated by Reader."));
#endif
			}
			break;
		default:
			break;
		}
	}

	virtual void OnLinkedSendRequestFail() final
	{
		if (WriterState == WriterStateEnum::Updating)
		{
			TrackedSurface->SetBlockPending(CurrentIndex);
		}
	}

private:
	void OnUpdating()
	{
		const uint8_t sendIndex = TrackedSurface->GetNextBlockPendingIndex(CurrentIndex);
		if (TrackedSurface->HasBlockPending())
		{
			if (sendIndex >= CurrentIndex)
			{
				const uint8_t nextIndex = sendIndex + 1;
				if (nextIndex > sendIndex
					&& (nextIndex < TrackedSurface->GetBlockCount())
					&& (TrackedSurface->GetNextBlockPendingIndex(nextIndex) == nextIndex))
				{
					// If the current and next blocks are adjacent, send both.
					if (RequestSend2xBlock(sendIndex))
					{
						TrackedSurface->ClearBlockPending(sendIndex);
						TrackedSurface->ClearBlockPending(nextIndex);
						CurrentIndex = nextIndex + 1;
					}
				}
				else if (RequestSend1xBlock(sendIndex))
				{
					TrackedSurface->ClearBlockPending(sendIndex);
					CurrentIndex = sendIndex + 1;
				}
			}
			else if (TrackedSurface->IsHot())
			{
				// Optimization: surface has already sync'd once
				// and we're looping around, skipping validation.
				WriterState = WriterStateEnum::Throttling;
				CurrentIndex = 0;
			}
			else
			{
				WriterState = WriterStateEnum::Validating;
				CurrentIndex = 0;
				ResetPacketThrottle();
			}
		}
		else
		{
			WriterState = WriterStateEnum::Validating;
			CurrentIndex = 0;
			ResetPacketThrottle();
		}
	}

	const bool RequestSend2xBlock(const uint8_t blockIndex)
	{
		const uint8_t* surfaceData = TrackedSurface->GetBlockData();
		const uint8_t indexOffset = blockIndex * ISurface::BytesPerBlock;

		OutPacket.SetPort(Port);
		OutPacket.SetHeader(WriterUpdateBlock2xDefinition::HEADER);
		OutPacket.Payload[WriterUpdateBlock2xDefinition::INDEX_OFFSET] = blockIndex;

		for (uint_fast8_t i = 0; i < (2 * ISurface::BytesPerBlock); i++)
		{
			OutPacket.Payload[WriterUpdateBlock2xDefinition::BLOCK1_OFFSET + i] = surfaceData[indexOffset + i];
		}

		return RequestSendPacket(WriterUpdateBlock2xDefinition::PAYLOAD_SIZE);
	}

	const bool RequestSend1xBlock(const uint8_t blockIndex)
	{
		const uint8_t* surfaceData = TrackedSurface->GetBlockData();
		const uint8_t indexOffset = blockIndex * ISurface::BytesPerBlock;

		OutPacket.SetPort(Port);
		OutPacket.SetHeader(WriterUpdateBlock1xDefinition::HEADER);
		OutPacket.Payload[WriterUpdateBlock1xDefinition::INDEX_OFFSET] = blockIndex;

		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			OutPacket.Payload[WriterUpdateBlock1xDefinition::BLOCKS_OFFSET + i] = surfaceData[indexOffset + i];
		}

		return RequestSendPacket(WriterUpdateBlock1xDefinition::PAYLOAD_SIZE);
	}
};
#endif