// SurfaceReader.h

#ifndef _SURFACE_READER_h
#define _SURFACE_READER_h

#include "AbstractSurfaceService.h"


using namespace SurfaceDefinitions;

template <const uint8_t Port, const uint32_t ServiceId>
class SurfaceReader : public AbstractSurfaceService<Port, ServiceId, SyncSurfaceMaxPayloadSize>
{
private:
	using BaseClass = AbstractSurfaceService<Port, ServiceId, SyncSurfaceMaxPayloadSize>;

	using BaseClass::FAST_CHECK_PERIOD_MILLIS;

	static constexpr uint32_t RECEIVE_FAILED_PERIOD_MILLIS = 100;
	static constexpr uint32_t RESEND_PERIOD_MICROS = 50000;

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
	uint32_t LastUpdateReceived = 0;
	ReaderStateEnum ReaderState = ReaderStateEnum::Disabled;

	bool NotifyRequested = false;

public:
	SurfaceReader(Scheduler& scheduler, ILoLaLink* link, ISurface* trackedSurface)
		: BaseClass(scheduler, link, trackedSurface)
	{}

	virtual void OnLinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case WriterCheckHashDefinition::HEADER:
			if (payloadSize == WriterCheckHashDefinition::PAYLOAD_SIZE)
			{
				switch (ReaderState)
				{
				case ReaderStateEnum::Sleeping:
				case ReaderStateEnum::Updating:
				case ReaderStateEnum::Invalidating:
					ReaderState = ReaderStateEnum::Validating;
					break;
				case ReaderStateEnum::Validating:
					ResetPacketThrottle();
					break;
				case ReaderStateEnum::Disabled:
				default:
					return;
					break;
				}
				SetRemoteHashArray(&payload[WriterCheckHashDefinition::CRC_OFFSET]);
				ResetPacketThrottle();
				Task::enableDelayed(0);
			}
			break;
		case WriterUpdateBlock1xDefinition::HEADER:
			if (payloadSize == WriterUpdateBlock1xDefinition::PAYLOAD_SIZE
				&& ReaderState != ReaderStateEnum::Disabled)
			{
				ReaderState = ReaderStateEnum::Updating;
				On1xBlockPacketReceived(payload);
			}
			break;
		case WriterUpdateBlock2xDefinition::HEADER:
			if (payloadSize == WriterUpdateBlock2xDefinition::PAYLOAD_SIZE
				&& ReaderState != ReaderStateEnum::Disabled)
			{
				ReaderState = ReaderStateEnum::Updating;
				On2xBlockPacketReceived(payload);
			}
			break;
		default:
			break;
		}
	}

protected:
	virtual void OnLinkedService() final
	{
		if (NotifyRequested)
		{
			NotifyRequested = false;
			Task::delay(0);

			if (!TrackedSurface->IsHot())
			{
				TrackedSurface->NotifyUpdated();
			}
		}
		else
		{
			switch (ReaderState)
			{
			case ReaderStateEnum::Updating:
				Task::delay(FAST_CHECK_PERIOD_MILLIS);
				if (TrackedSurface->IsHot() && HashesMatch())
				{
					ReaderState = ReaderStateEnum::Sleeping;
				}
				else if (millis() - LastUpdateReceived > RECEIVE_FAILED_PERIOD_MILLIS)
				{
					ReaderState = ReaderStateEnum::Invalidating;
				}
				break;
			case ReaderStateEnum::Validating:
				Task::delay(0);
				if (PacketThrottle()
					&& RequestSendMeta(ReaderValidateDefinition::HEADER))
				{
					ReaderState = ReaderStateEnum::Sleeping;
				}
				break;
			case ReaderStateEnum::Invalidating:
				Task::delay(0);
				if (PacketThrottle(RESEND_PERIOD_MICROS))
				{
					RequestSendMeta(ReaderInvalidateDefinition::HEADER);
				}
				break;
			case ReaderStateEnum::Sleeping:
			case ReaderStateEnum::Disabled:
			default:
				Task::disable();
				break;
			}
		}
	}

	virtual void OnServiceStarted() final
	{
		BaseClass::OnServiceStarted();
		if (SurfaceSize > 0)
		{
			ReaderState = ReaderStateEnum::Updating;
			NotifyRequested = false;
			LastUpdateReceived = millis();
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
	void CheckForSurfaceHeat()
	{
		if (!TrackedSurface->IsHot()
			&& !TrackedSurface->HasBlockPending())
		{
			TrackedSurface->SetHot(true);
			NotifyRequested = true;
			Task::enableDelayed(0);
#if defined(DEBUG_LOLA)
			Serial.println(F("Reader is Hot."));
#endif
		}
	}

	void On2xBlockPacketReceived(const uint8_t* payload)
	{
		const uint8_t blockIndex = payload[WriterUpdateBlock2xDefinition::INDEX_OFFSET];
		const uint8_t indexOffset = blockIndex * ISurface::BytesPerBlock;

		uint8_t* surfaceData = TrackedSurface->GetBlockData();
		for (uint_fast8_t i = 0; i < (2 * ISurface::BytesPerBlock); i++)
		{
			surfaceData[indexOffset + i] = payload[WriterUpdateBlock2xDefinition::BLOCK1_OFFSET + i];
		}

		if (!TrackedSurface->IsHot())
		{
			TrackedSurface->ClearBlockPending(blockIndex);
			TrackedSurface->ClearBlockPending(blockIndex + 1);
			CheckForSurfaceHeat();
		}
		else
		{
			NotifyRequested = true;
		}

		LastUpdateReceived = millis();
	}

	void On1xBlockPacketReceived(const uint8_t* payload)
	{
		const uint8_t blockIndex = payload[WriterUpdateBlock1xDefinition::INDEX_OFFSET];
		const uint8_t indexOffset = blockIndex * ISurface::BytesPerBlock;

		uint8_t* surfaceData = TrackedSurface->GetBlockData();
		for (uint_fast8_t i = 0; i < ISurface::BytesPerBlock; i++)
		{
			surfaceData[indexOffset + i] = payload[WriterUpdateBlock1xDefinition::BLOCKS_OFFSET + i];
		}

		if (!TrackedSurface->IsHot())
		{
			TrackedSurface->ClearBlockPending(blockIndex);
			CheckForSurfaceHeat();
		}
		else
		{
			NotifyRequested = true;
		}


		LastUpdateReceived = millis();
	}
};
#endif