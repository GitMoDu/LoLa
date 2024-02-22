// SurfaceReader.h

#ifndef _SURFACE_READER_h
#define _SURFACE_READER_h

#include "AbstractSurfaceService.h"

/// <summary>
/// Surface Reader reflects data blocks from the partner Surface Writer.
/// Updates block data and notifies listeners of change..
/// Surface is hot sync is active and all blocks have been reflected at least once.
/// CRC validated with Fletcher 16.
/// </summary>
/// <typeparam name="Port">The port registered for this service.</typeparam>
/// <typeparam name="ServiceId">Unique service identifier.</typeparam>
template <const uint8_t Port, const uint32_t ServiceId>
class SurfaceReader : public AbstractSurfaceService<Port, ServiceId, SurfaceReaderMaxPayloadSize>
{
private:
	using BaseClass = AbstractSurfaceService<Port, ServiceId, SurfaceReaderMaxPayloadSize>;

	static constexpr uint8_t RECEIVE_FAILED_PERIOD_MILLIS = 50;
	static constexpr uint8_t RESEND_PERIOD_MILLIS = RECEIVE_FAILED_PERIOD_MILLIS / 2;

	enum class ReaderStateEnum : uint8_t
	{
		Disabled,
		Updating,
		Validating,
		Invalidating,
		Sleeping
	};

protected:
	using BaseClass::ResetPacketThrottle;
	using BaseClass::PacketThrottle;

	using BaseClass::Surface;
	using BaseClass::SurfaceSize;
	using BaseClass::BlockData;
	using BaseClass::SetRemoteHashArray;
	using BaseClass::HashesMatch;
	using BaseClass::RequestSendMeta;

private:
	uint32_t LastReceived = 0;
	ReaderStateEnum ReaderState = ReaderStateEnum::Disabled;
	bool NotifyRequested = false;

public:
	SurfaceReader(Scheduler& scheduler, ILoLaLink* link, ISurface* surface)
		: BaseClass(scheduler, link, surface)
	{}

	virtual void OnServiceStarted() final
	{
		BaseClass::OnServiceStarted();
		if (SurfaceSize > 0)
		{
			ReaderState = ReaderStateEnum::Sleeping;
			NotifyRequested = false;
			LastReceived = millis();
			Task::disable();
			Surface->SetHot(false);
			Surface->NotifyUpdated();
		}
	}

	virtual void OnServiceEnded() final
	{
		BaseClass::OnServiceEnded();
		ReaderState = ReaderStateEnum::Disabled;
	}

	virtual void OnLinkedService() final
	{
		if (NotifyRequested)
		{
			NotifyRequested = false;
			Surface->NotifyUpdated();
		}
		else switch (ReaderState)
		{
		case ReaderStateEnum::Updating:
			if (Surface->IsHot() && HashesMatch())
			{
				ReaderState = ReaderStateEnum::Sleeping;
				Task::delay(0);
			}
			else if (millis() - LastReceived > RECEIVE_FAILED_PERIOD_MILLIS)
			{
				ReaderState = ReaderStateEnum::Invalidating;
				ResetPacketThrottle();
				Task::delay(0);
			}
			else { Task::delay(1); }
			break;
		case ReaderStateEnum::Validating:
			Task::delay(0);
			if (Surface->IsHot())
			{
				if (RequestSendMeta(ReaderValidateDefinition::HEADER))
				{
					if (HashesMatch())
					{
						ReaderState = ReaderStateEnum::Sleeping;
					}
					else
					{
						ReaderState = ReaderStateEnum::Updating;
					}
				}
			}
			else if (Surface->HasBlockPending())
			{
				ReaderState = ReaderStateEnum::Invalidating;
				ResetPacketThrottle();
			}
			else if (RequestSendMeta(ReaderValidateDefinition::HEADER))
			{
				ReaderState = ReaderStateEnum::Sleeping;
			}
			break;
		case ReaderStateEnum::Invalidating:
			if (PacketThrottle(1000 * RESEND_PERIOD_MILLIS))
			{
				RequestSendMeta(ReaderInvalidateDefinition::HEADER);
			}
			else { Task::delay(0); }
			break;
		case ReaderStateEnum::Sleeping:
		case ReaderStateEnum::Disabled:
		default:
			Task::disable();
			break;
		}
	}

	virtual void OnLinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) final
	{
		if (ReaderState != ReaderStateEnum::Disabled)
		{
			const uint8_t header = payload[HeaderDefinition::HEADER_INDEX];
			switch (header)
			{
			case WriterCheckHashDefinition::HEADER:
				if (payloadSize == WriterCheckHashDefinition::PAYLOAD_SIZE)
				{
					LastReceived = millis();
					switch (ReaderState)
					{
					case ReaderStateEnum::Sleeping:
						Task::enableDelayed(0);
					case ReaderStateEnum::Updating:
					case ReaderStateEnum::Invalidating:
						ReaderState = ReaderStateEnum::Validating;
						ResetPacketThrottle();
						break;
					default:
						break;
					}
					SetRemoteHashArray(&payload[WriterCheckHashDefinition::CRC_OFFSET]);
				}
				break;
			case WriterDefinition1x1::HEADER:
				if (payloadSize == WriterDefinition1x1::PAYLOAD_SIZE)
				{
					LastReceived = millis();
					Task::enableDelayed(0);
					ReaderState = ReaderStateEnum::Updating;
					OnReceived1x1(payload);
				}
				break;
			case WriterDefinition2x1::HEADER:
				if (payloadSize == WriterDefinition2x1::PAYLOAD_SIZE)
				{
					LastReceived = millis();
					Task::enableDelayed(0);
					ReaderState = ReaderStateEnum::Updating;
					OnReceived2x1(payload);
				}
				break;
			default:
				if (payloadSize == WriterDefinition1x2::PAYLOAD_SIZE
					&& header >= WriterDefinition1x2::HEADER
					&& header < WriterDefinition1x2::MAX_HEADER)
				{
					LastReceived = millis();
					Task::enableDelayed(0);
					ReaderState = ReaderStateEnum::Updating;
					OnReceived1x2(payload, header);
				}
				else if (payloadSize == WriterDefinition1x3::PAYLOAD_SIZE
					&& header >= WriterDefinition1x3::HEADER
					&& header < WriterDefinition1x3::MAX_HEADER)
				{
					LastReceived = millis();
					Task::enableDelayed(0);
					ReaderState = ReaderStateEnum::Updating;
					OnReceived1x3(payload, header);
				}
				break;
			}
		}
	}

private:
	void OnReceived1x1(const uint8_t* payload)
	{
		const uint8_t blockIndex = payload[WriterDefinition1x1::INDEX_OFFSET];
		const uint8_t indexOffset = blockIndex * ISurface::BytesPerBlock;

		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			BlockData[indexOffset + i] = payload[WriterDefinition1x1::BLOCK_OFFSET + i];
		}

		if (!Surface->IsHot())
		{
			Surface->ClearBlockPending(blockIndex);
			if (!Surface->HasBlockPending())
			{
				Surface->SetHot(true);
				NotifyRequested = true;
			}
		}
		else
		{
			NotifyRequested = true;
		}
	}

	void OnReceived2x1(const uint8_t* payload)
	{
		const uint8_t block0Index = payload[WriterDefinition2x1::INDEX0_OFFSET];
		const uint8_t block1Index = payload[WriterDefinition2x1::INDEX1_OFFSET];
		const uint8_t index0Offset = block0Index * ISurface::BytesPerBlock;
		const uint8_t index1Offset = block1Index * ISurface::BytesPerBlock;

		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			BlockData[index0Offset + i] = payload[WriterDefinition2x1::BLOCK0_OFFSET + i];
			BlockData[index1Offset + i] = payload[WriterDefinition2x1::BLOCK1_OFFSET + i];
		}

		if (!Surface->IsHot())
		{
			Surface->ClearBlockPending(block0Index);
			Surface->ClearBlockPending(block1Index);
			if (!Surface->HasBlockPending())
			{
				Surface->SetHot(true);
				NotifyRequested = true;
			}
		}
		else
		{
			NotifyRequested = true;
		}
	}

	void OnReceived1x2(const uint8_t* payload, const uint8_t header)
	{
		const uint8_t block0Index = WriterDefinition1x2::GetEmbeddedValueFromHeader(header);
		const uint8_t block1Index = block0Index + 1;
		const uint8_t index0Offset = block0Index * ISurface::BytesPerBlock;
		const uint8_t index1Offset = index0Offset + ISurface::BytesPerBlock;

		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			BlockData[index0Offset + i] = payload[WriterDefinition1x2::BLOCK0_OFFSET + i];
			BlockData[index1Offset + i] = payload[WriterDefinition1x2::BLOCK1_OFFSET + i];
		}

		if (!Surface->IsHot())
		{
			Surface->ClearBlockPending(block0Index);
			Surface->ClearBlockPending(block1Index);
			if (!Surface->HasBlockPending())
			{
				Surface->SetHot(true);
				NotifyRequested = true;
			}
		}
		else
		{
			NotifyRequested = true;
		}
	}

	void OnReceived1x3(const uint8_t* payload, const uint8_t header)
	{
		const uint8_t block0Index = WriterDefinition1x3::GetEmbeddedValueFromHeader(header);
		const uint8_t block1Index = block0Index + 1;
		const uint8_t block2Index = block1Index + 1;
		const uint8_t index0Offset = block0Index * ISurface::BytesPerBlock;
		const uint8_t index1Offset = index0Offset + ISurface::BytesPerBlock;
		const uint8_t index2Offset = index1Offset + ISurface::BytesPerBlock;

		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			BlockData[index0Offset + i] = payload[WriterDefinition1x3::BLOCK0_OFFSET + i];
			BlockData[index1Offset + i] = payload[WriterDefinition1x3::BLOCK1_OFFSET + i];
			BlockData[index2Offset + i] = payload[WriterDefinition1x3::BLOCK2_OFFSET + i];
		}

		if (!Surface->IsHot())
		{
			Surface->ClearBlockPending(block0Index);
			Surface->ClearBlockPending(block1Index);
			Surface->ClearBlockPending(block2Index);
			if (!Surface->HasBlockPending())
			{
				Surface->SetHot(true);
				NotifyRequested = true;
			}
		}
		else
		{
			NotifyRequested = true;
		}
	}
};
#endif