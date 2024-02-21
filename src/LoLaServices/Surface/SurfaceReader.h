// SurfaceReader.h

#ifndef _SURFACE_READER_h
#define _SURFACE_READER_h

#include "AbstractSurfaceService.h"

template <const uint8_t Port, const uint32_t ServiceId>
class SurfaceReader : public AbstractSurfaceService<Port, ServiceId, SurfaceReaderMaxPayloadSize>
{
private:
	using BaseClass = AbstractSurfaceService<Port, ServiceId, SurfaceReaderMaxPayloadSize>;

	static constexpr uint32_t START_DELAY_PERIOD_MILLIS = 100;
	static constexpr uint32_t RECEIVE_FAILED_PERIOD_MILLIS = 50;
	static constexpr uint32_t RESEND_PERIOD_MICROS = 30000;

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

	using BaseClass::TrackedSurface;
	using BaseClass::SurfaceSize;
	using BaseClass::SetRemoteHashArray;
	using BaseClass::HashesMatch;
	using BaseClass::RequestSendMeta;

private:
	uint32_t LastBlockReceived = 0;
	ReaderStateEnum ReaderState = ReaderStateEnum::Disabled;

	bool NotifyRequested = false;

public:
	SurfaceReader(Scheduler& scheduler, ILoLaLink* link, ISurface* trackedSurface)
		: BaseClass(scheduler, link, trackedSurface)
	{}

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
					switch (ReaderState)
					{
					case ReaderStateEnum::Sleeping:
						Task::enableDelayed(0);
					case ReaderStateEnum::Updating:
					case ReaderStateEnum::Invalidating:
						ReaderState = ReaderStateEnum::Validating;
						break;
					default:
						break;
					}
					SetRemoteHashArray(&payload[WriterCheckHashDefinition::CRC_OFFSET]);
					ResetPacketThrottle();
				}
				break;
			case WriterDefinition1x1::HEADER:
				if (payloadSize == WriterDefinition1x1::PAYLOAD_SIZE)
				{
					LastBlockReceived = millis();
					Task::enableDelayed(0);
					ReaderState = ReaderStateEnum::Updating;
					OnReceived1x1(payload);
				}
				break;
			case WriterDefinition2x1::HEADER:
				if (payloadSize == WriterDefinition2x1::PAYLOAD_SIZE)
				{
					LastBlockReceived = millis();
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
					LastBlockReceived = millis();
					Task::enableDelayed(0);
					ReaderState = ReaderStateEnum::Updating;
					OnReceived1x2(payload, header);
				}
				else if (payloadSize == WriterDefinition1x3::PAYLOAD_SIZE
					&& header >= WriterDefinition1x3::HEADER
					&& header < WriterDefinition1x3::MAX_HEADER)
				{
					LastBlockReceived = millis();
					Task::enableDelayed(0);
					ReaderState = ReaderStateEnum::Updating;
					OnReceived1x3(payload, header);
				}
				break;
			}
		}
	}

protected:
	virtual void OnLinkedService() final
	{
		const uint32_t timestamp = millis();

		if (NotifyRequested)
		{
			NotifyRequested = false;
			Task::delay(0);
			if (!TrackedSurface->IsHot())
			{
				TrackedSurface->NotifyUpdated();
			}
		}
		else switch (ReaderState)
		{
		case ReaderStateEnum::Updating:
			if (TrackedSurface->IsHot() && HashesMatch())
			{
				ReaderState = ReaderStateEnum::Sleeping;
				Task::delay(0);
			}
			else if (timestamp - LastBlockReceived > RECEIVE_FAILED_PERIOD_MILLIS)
			{
				ReaderState = ReaderStateEnum::Invalidating;
				Task::delay(0);
			}
			else { Task::delay(1); }
			break;
		case ReaderStateEnum::Validating:
			Task::enableDelayed(0);
			if (PacketThrottle())
			{
				if (TrackedSurface->IsHot())
				{
					if (RequestSendMeta(ReaderValidateDefinition::HEADER))
					{
						ReaderState = ReaderStateEnum::Sleeping;
					}
				}
				else if (HashesMatch())
				{
					if (!TrackedSurface->HasBlockPending())
					{
						TrackedSurface->SetHot(true);
						NotifyRequested = true;
						Task::enableDelayed(0);
#if defined(DEBUG_LOLA)
						Serial.println(F("Reader is Hot."));
#endif
						if (RequestSendMeta(ReaderValidateDefinition::HEADER))
						{
							ReaderState = ReaderStateEnum::Sleeping;
						}
					}
					else if (RequestSendMeta(ReaderInvalidateDefinition::HEADER))
					{
						ReaderState = ReaderStateEnum::Sleeping;
					}
				}
				else if (RequestSendMeta(ReaderInvalidateDefinition::HEADER))
				{
					ReaderState = ReaderStateEnum::Sleeping;
				}
			}
			break;
		case ReaderStateEnum::Invalidating:
			if (PacketThrottle(RESEND_PERIOD_MICROS))
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

	virtual void OnServiceStarted() final
	{
		BaseClass::OnServiceStarted();
		if (SurfaceSize > 0)
		{
			ReaderState = ReaderStateEnum::Updating;
			NotifyRequested = false;
			LastBlockReceived = millis();
			Task::delay(START_DELAY_PERIOD_MILLIS);
		}
	}

	virtual void OnServiceEnded() final
	{
		BaseClass::OnServiceEnded();
		ReaderState = ReaderStateEnum::Disabled;
#if defined(DEBUG_LOLA)
		Serial.println(F("Reader is Cold."));
#endif
	}

private:
	void OnReceived1x1(const uint8_t* payload)
	{
		const uint8_t blockIndex = payload[WriterDefinition1x1::INDEX_OFFSET];
		const uint8_t indexOffset = blockIndex * ISurface::BytesPerBlock;

		uint8_t* surfaceData = TrackedSurface->GetBlockData();
		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			surfaceData[indexOffset + i] = payload[WriterDefinition1x1::BLOCK_OFFSET + i];
		}

		if (!TrackedSurface->IsHot())
		{
			TrackedSurface->ClearBlockPending(blockIndex);
		}
		else
		{
			NotifyRequested = true;
		}

		Serial.print(F("\t1x1\t"));
		Serial.println(blockIndex);
	}

	void OnReceived2x1(const uint8_t* payload)
	{
		const uint8_t block0Index = payload[WriterDefinition2x1::INDEX0_OFFSET];
		const uint8_t block1Index = payload[WriterDefinition2x1::INDEX1_OFFSET];
		const uint8_t index0Offset = block0Index * ISurface::BytesPerBlock;
		const uint8_t index1Offset = block1Index * ISurface::BytesPerBlock;

		uint8_t* surfaceData = TrackedSurface->GetBlockData();
		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			surfaceData[index0Offset + i] = payload[WriterDefinition2x1::BLOCK0_OFFSET + i];
			surfaceData[index1Offset + i] = payload[WriterDefinition2x1::BLOCK1_OFFSET + i];
		}

		if (!TrackedSurface->IsHot())
		{
			TrackedSurface->ClearBlockPending(block0Index);
			TrackedSurface->ClearBlockPending(block1Index);
		}
		else
		{
			NotifyRequested = true;
		}

		Serial.print(F("\t2x1\t"));
		Serial.print(block0Index);
		Serial.print(',');
		Serial.println(block1Index);
	}

	void OnReceived1x2(const uint8_t* payload, const uint8_t header)
	{
		const uint8_t block0Index = WriterDefinition1x2::GetEmbeddedValueFromHeader(header);
		const uint8_t block1Index = block0Index + 1;
		const uint8_t index0Offset = block0Index * ISurface::BytesPerBlock;
		const uint8_t index1Offset = index0Offset + ISurface::BytesPerBlock;

		uint8_t* surfaceData = TrackedSurface->GetBlockData();
		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			surfaceData[index0Offset + i] = payload[WriterDefinition1x2::BLOCK0_OFFSET + i];
			surfaceData[index1Offset + i] = payload[WriterDefinition1x2::BLOCK1_OFFSET + i];
		}

		if (!TrackedSurface->IsHot())
		{
			TrackedSurface->ClearBlockPending(block0Index);
			TrackedSurface->ClearBlockPending(block1Index);
		}
		else
		{
			NotifyRequested = true;
		}

		Serial.print(F("\t1x2\t"));
		Serial.print(block0Index);
		Serial.print(',');
		Serial.println(block1Index);
	}

	void OnReceived1x3(const uint8_t* payload, const uint8_t header)
	{
		const uint8_t block0Index = WriterDefinition1x3::GetEmbeddedValueFromHeader(header);
		const uint8_t block1Index = block0Index + 1;
		const uint8_t block2Index = block1Index + 1;
		const uint8_t index0Offset = block0Index * ISurface::BytesPerBlock;
		const uint8_t index1Offset = index0Offset + ISurface::BytesPerBlock;
		const uint8_t index2Offset = index1Offset + ISurface::BytesPerBlock;

		uint8_t* surfaceData = TrackedSurface->GetBlockData();
		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			surfaceData[index0Offset + i] = payload[WriterDefinition1x3::BLOCK0_OFFSET + i];
			surfaceData[index1Offset + i] = payload[WriterDefinition1x3::BLOCK1_OFFSET + i];
			surfaceData[index2Offset + i] = payload[WriterDefinition1x3::BLOCK2_OFFSET + i];
		}

		if (!TrackedSurface->IsHot())
		{
			TrackedSurface->ClearBlockPending(block0Index);
			TrackedSurface->ClearBlockPending(block1Index);
			TrackedSurface->ClearBlockPending(block2Index);
		}
		else
		{
			NotifyRequested = true;
		}

		Serial.print(F("\t1x3\t"));
		Serial.print(block0Index);
		Serial.print(',');
		Serial.print(block1Index);
		Serial.print(',');
		Serial.println(block2Index);
	}
};
#endif