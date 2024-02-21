// SurfaceWriter.h

#ifndef _SURFACE_WRITER_h
#define _SURFACE_WRITER_h

#include "AbstractSurfaceService.h"

template <const uint8_t Port
	, const uint32_t ServiceId
	, const uint16_t ThrottlePeriodMillis = 50>
class SurfaceWriter
	: public AbstractSurfaceService<Port, ServiceId, SurfaceWriterMaxPayloadSize>
	, public virtual ISurfaceListener
{
private:
	using BaseClass = AbstractSurfaceService<Port, ServiceId, SurfaceWriterMaxPayloadSize>;

	static constexpr uint32_t START_DELAY_PERIOD_MILLIS = 50;
	static constexpr uint8_t MAX_RETRIES_BEFORE_INVALIDATION = 2;

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

	using BaseClass::Surface;
	using BaseClass::SurfaceSize;
	using BaseClass::BlockData;
	using BaseClass::HashesMatch;
	using BaseClass::SetRemoteHashArray;
	using BaseClass::GetLocalHash;
	using BaseClass::InvalidateRemoteHash;
	using BaseClass::RequestSendMeta;

private:
	uint32_t ThrottleStart = 0;
	uint16_t MatchHash = 0;
	WriterStateEnum WriterState = WriterStateEnum::Disabled;
	uint8_t CurrentIndex = 0;
	uint8_t MatchTryCount = 0;

public:
	SurfaceWriter(Scheduler& scheduler, ILoLaLink* link, ISurface* surface)
		: ISurfaceListener()
		, BaseClass(scheduler, link, surface)
	{}

public:
	virtual const bool Setup() final
	{
		if (LoLaPacketDefinition::MAX_PAYLOAD_SIZE >= SurfaceWriterMaxPayloadSize
			&& BaseClass::Setup())
		{
			return Surface->AttachSurfaceListener(this);
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
			MatchTryCount = 0;
			Task::enableDelayed(START_DELAY_PERIOD_MILLIS);
			ThrottleStart = millis();
		}
	}

	virtual void OnServiceEnded() final
	{
		BaseClass::OnServiceEnded();
		if (SurfaceSize > 0)
		{
			WriterState = WriterStateEnum::Disabled;
		}
	}

	virtual void OnSurfaceUpdated(const bool hot) final
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
			if (Surface->IsHot()
				&& HashesMatch())
			{
				Surface->ClearAllBlocksPending();
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
			if (PacketThrottle())
			{
				if (Surface->IsHot())
				{
					if (!RequestSendMeta(WriterCheckHashDefinition::HEADER))
					{
						Task::delay(0);
					}
				}
				else
				{
					MatchHash = GetLocalHash();
					if (RequestSendMeta(MatchHash, WriterCheckHashDefinition::HEADER))
					{
						Task::delay(0);
					}
				}
			}
			else
			{
				Task::delay(1);
			}
			break;
		case WriterStateEnum::Matching:
			if (!Surface->IsHot())
			{
				if (HashesMatch(MatchHash))
				{
					Surface->SetHot(true);
					Surface->NotifyUpdated();
				}
			}

			WriterState = WriterStateEnum::Throttling;
			MatchTryCount++;
			Task::delay(0);
			if (!HashesMatch()
				&& (MatchTryCount > MAX_RETRIES_BEFORE_INVALIDATION
					|| !Surface->HasBlockPending()))
			{
				// Sync has been invalidated.
				InvalidateRemoteHash();
				Surface->SetAllBlocksPending();
			}
			break;
		case WriterStateEnum::Throttling:
			if (timestamp - ThrottleStart >= ThrottlePeriodMillis)
			{
				Task::delay(0);
				CurrentIndex = 0;
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
		if (WriterState != WriterStateEnum::Disabled)
		{
			switch (payload[HeaderDefinition::HEADER_INDEX])
			{
			case ReaderValidateDefinition::HEADER:
				if (payloadSize == ReaderValidateDefinition::PAYLOAD_SIZE)
				{
					SetRemoteHashArray(&payload[ReaderValidateDefinition::CRC_OFFSET]);

					switch (WriterState)
					{
					case WriterStateEnum::Sleeping:
						Task::enableDelayed(0);
					case WriterStateEnum::Validating:
						WriterState = WriterStateEnum::Matching;
						break;
					default:
						break;
					}
				}
				break;
			case ReaderInvalidateDefinition::HEADER:
				if (payloadSize == ReaderInvalidateDefinition::PAYLOAD_SIZE)
				{
					Serial.println(F("W Inv"));
					SetRemoteHashArray(&payload[ReaderInvalidateDefinition::CRC_OFFSET]);

					switch (WriterState)
					{
					case WriterStateEnum::Sleeping:
						Task::enableDelayed(0);
					case WriterStateEnum::Matching:
					case WriterStateEnum::Validating:
						WriterState = WriterStateEnum::Throttling;
					case WriterStateEnum::Updating:
					case WriterStateEnum::Throttling:
						CurrentIndex = 0;
						Surface->SetAllBlocksPending();
						break;
					default:
						break;
					}
				}
				break;
			default:
				break;
			}
		}
	}

	virtual void OnLinkedSendRequestFail() final
	{
		if (WriterState == WriterStateEnum::Updating)
		{
			Surface->SetBlockPending(CurrentIndex);
		}
	}

private:
	void OnUpdating()
	{
		const uint8_t index = Surface->GetNextBlockPendingIndex(CurrentIndex);
		if (Surface->HasBlockPending()
			&& index >= CurrentIndex)
		{
			// Pick the best request type, based on pending block count and indexes.
			const uint8_t next = Surface->GetNextBlockPendingIndex(index + 1);

			if (next > index)
			{
				if (next == (index + 1))
				{
					const uint8_t afterNext = Surface->GetNextBlockPendingIndex(next + 1);

					if (afterNext > next
						&& afterNext == (next + 1))
					{
						if (RequestSend1x3(index))
						{
							Surface->ClearBlockPending(index);
							Surface->ClearBlockPending(next);
							Surface->ClearBlockPending(afterNext);
							CurrentIndex = afterNext + 1;
						}
					}
					else if (RequestSend1x2(index))
					{
						Surface->ClearBlockPending(index);
						Surface->ClearBlockPending(next);
						CurrentIndex = next + 1;
					}
				}
				else if (RequestSend2x1(index, next))
				{
					Surface->ClearBlockPending(index);
					Surface->ClearBlockPending(next);
					CurrentIndex = next + 1;
				}
			}
			else if (RequestSend1x1(index))
			{
				Surface->ClearBlockPending(index);
				CurrentIndex = index + 1;
			}
		}
		else
		{
			WriterState = WriterStateEnum::Validating;
			MatchTryCount = 0;
			ResetPacketThrottle();
		}
	}

	const bool RequestSend1x1(const uint8_t blockIndex)
	{
		const uint8_t indexOffset = blockIndex * ISurface::BytesPerBlock;

		OutPacket.SetPort(Port);
		OutPacket.SetHeader(WriterDefinition1x1::HEADER);
		OutPacket.Payload[WriterDefinition1x1::INDEX_OFFSET] = blockIndex;

		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			OutPacket.Payload[WriterDefinition1x1::BLOCK_OFFSET + i] = BlockData[indexOffset + i];
		}

		return RequestSendPacket(WriterDefinition1x1::PAYLOAD_SIZE);
	}

	const bool RequestSend2x1(const uint8_t block0Index, const uint8_t block1Index)
	{
		const uint8_t index0Offset = block0Index * ISurface::BytesPerBlock;
		const uint8_t index1Offset = block1Index * ISurface::BytesPerBlock;

		OutPacket.SetPort(Port);
		OutPacket.SetHeader(WriterDefinition2x1::HEADER);
		OutPacket.Payload[WriterDefinition2x1::INDEX0_OFFSET] = block0Index;
		OutPacket.Payload[WriterDefinition2x1::INDEX1_OFFSET] = block1Index;

		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			OutPacket.Payload[WriterDefinition2x1::BLOCK0_OFFSET + i] = BlockData[index0Offset + i];
			OutPacket.Payload[WriterDefinition2x1::BLOCK1_OFFSET + i] = BlockData[index1Offset + i];
		}

		return RequestSendPacket(WriterDefinition2x1::PAYLOAD_SIZE);
	}

	const bool RequestSend1x2(const uint8_t block0Index)
	{
		const uint8_t index0Offset = block0Index * ISurface::BytesPerBlock;
		const uint8_t index1Offset = index0Offset + ISurface::BytesPerBlock;

		OutPacket.SetPort(Port);
		OutPacket.SetHeader(WriterDefinition1x2::GetHeaderFromEmbeddedValue(block0Index));
		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			OutPacket.Payload[WriterDefinition1x2::BLOCK0_OFFSET + i] = BlockData[index0Offset + i];
			OutPacket.Payload[WriterDefinition1x2::BLOCK1_OFFSET + i] = BlockData[index1Offset + i];
		}

		return RequestSendPacket(WriterDefinition1x2::PAYLOAD_SIZE);
	}

	const bool RequestSend1x3(const uint8_t block0Index)
	{
		const uint8_t index0Offset = block0Index * ISurface::BytesPerBlock;
		const uint8_t index1Offset = index0Offset + ISurface::BytesPerBlock;
		const uint8_t index2Offset = index1Offset + ISurface::BytesPerBlock;

		OutPacket.SetPort(Port);
		OutPacket.SetHeader(WriterDefinition1x3::GetHeaderFromEmbeddedValue(block0Index));
		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			OutPacket.Payload[WriterDefinition1x3::BLOCK0_OFFSET + i] = BlockData[index0Offset + i];
			OutPacket.Payload[WriterDefinition1x3::BLOCK1_OFFSET + i] = BlockData[index1Offset + i];
			OutPacket.Payload[WriterDefinition1x3::BLOCK2_OFFSET + i] = BlockData[index2Offset + i];
		}

		return RequestSendPacket(WriterDefinition1x3::PAYLOAD_SIZE);
	}
};
#endif