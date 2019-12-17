// SyncSurfaceBase.h

#ifndef _SYNCSURFACEBASE_h
#define _SYNCSURFACEBASE_h

#include <LoLaDefinitions.h>
#include <Services\SyncSurface\AbstractSync.h>
#include <Services\SyncSurface\SyncPacketDefinitions.h>
#include <Packet\LoLaPacket.h>
#include <Packet\PacketDefinition.h>
#include <Packet\LoLaPacketMap.h>

template<const uint32_t ThrottlePeriodMillis = 1>
class SyncSurfaceBase : public AbstractSync
{
private:
	PacketDefinition* SyncMetaDefinition = nullptr;
	PacketDefinition* DataPacketDefinition = nullptr;

	static const uint8_t SYNC_META_SUB_HEADER_SERVICE_DISCOVERY = 0;
	static const uint8_t SYNC_META_SUB_HEADER_UPDATE_FINISHED = 1;
	static const uint8_t SYNC_META_SUB_HEADER_UPDATE_FINISHED_REPLY = 2;
	static const uint8_t SYNC_META_SUB_HEADER_INVALIDATE_REQUEST = 3;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} ATUI;

	TemplateLoLaPacket<LOLA_PACKET_MIN_PACKET_SIZE + PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE> PacketHolder;

public:
	SyncSurfaceBase(Scheduler* scheduler, ILoLaDriver* driver, ITrackedSurface* trackedSurface,
		PacketDefinition* metaDefinition, PacketDefinition* dataDefinition)
		: AbstractSync(scheduler, ABSTRACT_SURFACE_FAST_CHECK_PERIOD_MILLIS, driver, trackedSurface, &PacketHolder)
	{
		SyncMetaDefinition = metaDefinition;
		DataPacketDefinition = dataDefinition;
//#ifdef DEBUG_LOLA
//		SyncMetaDefinition->SetOwner(trackedSurface);
//		DataPacketDefinition->SetOwner(trackedSurface);
//#endif
	}

	bool Setup()
	{
		if (!AbstractSync::Setup())
		{
			return false;
		}

		if (!LoLaDriver->GetPacketMap()->AddMapping(SyncMetaDefinition) ||
			!LoLaDriver->GetPacketMap()->AddMapping(DataPacketDefinition))
		{
			return false;
		}

		return true;
	}


protected:
	//Reader
	virtual void OnBlockReceived(const uint8_t index, uint8_t* payload) {}
	virtual void OnUpdateFinishedReceived() {}
	virtual void OnSyncFinishedReceived() {}

	//Writer
	virtual void OnServiceDiscoveryReceived(const uint8_t surfaceId) {}
	virtual void OnUpdateFinishedReplyReceived() {}
	virtual void OnInvalidateRequestReceived() {}

protected:

	bool CheckThrottling()
	{
		if (LastSyncMillis == 0
			||
			(millis() - LastSyncMillis > ThrottlePeriodMillis))
		{

			return true;
		}
		else
		{
			return false;
		}
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket)
	{
		if (incomingPacket->GetDataHeader() == DataPacketDefinition->GetHeader())
		{
			//To Reader.
			if (SyncState != SyncStateEnum::Disabled)
			{
				OnBlockReceived(incomingPacket->GetId(), incomingPacket->GetPayload());
			}

			return true;
		}
		else if (incomingPacket->GetDataHeader() == SyncMetaDefinition->GetHeader())
		{
			if (SyncState != SyncStateEnum::Disabled)
			{
				switch (incomingPacket->GetId())
				{
					//To Reader.
				case SYNC_META_SUB_HEADER_UPDATE_FINISHED:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
					Serial.println(F("OnUpdateFinishedReceived"));
#endif
					SetRemoteHash(incomingPacket->GetPayload()[0]);
					OnUpdateFinishedReceived();
					break;

					//To Writer.
				case SYNC_META_SUB_HEADER_UPDATE_FINISHED_REPLY:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
					Serial.println(F("OnUpdateFinishedReplyReceived"));
#endif
					SetRemoteHash(incomingPacket->GetPayload()[0]);
					OnUpdateFinishedReplyReceived();
					break;
				case SYNC_META_SUB_HEADER_INVALIDATE_REQUEST:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
					Serial.println(F("OnInvalidateReceived"));
#endif
					SetRemoteHash(incomingPacket->GetPayload()[0]);
					OnInvalidateRequestReceived();
					break;
				case SYNC_META_SUB_HEADER_SERVICE_DISCOVERY:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
					Serial.println(F("OnServiceDiscoveryReceived"));
#endif
					OnServiceDiscoveryReceived(incomingPacket->GetPayload()[0]);
					break;
				default:
					break;
				}
			}

			return true;
		}

		return false;
	}

	void UpdateBlockData(const uint8_t blockIndex, uint8_t* payload)
	{
		uint8_t IndexOffset = blockIndex * ITrackedSurface::BytesPerBlock;

		for (uint8_t i = 0; i < ITrackedSurface::BytesPerBlock; i++)
		{
			TrackedSurface->GetData()[IndexOffset + i] = payload[i];
		}
	}

	void PrepareBlockPacketPayload(const uint8_t index, uint8_t* payload)
	{
		uint8_t IndexOffset = index * ITrackedSurface::BytesPerBlock;

		for (uint8_t i = 0; i < ITrackedSurface::BytesPerBlock; i++)
		{
			payload[i] = TrackedSurface->GetData()[IndexOffset + i];
		}
	}

	inline void PrepareMetaHashPacket(const uint8_t subHeader)
	{
		Packet->SetDefinition(SyncMetaDefinition);
		Packet->SetId(subHeader);
		UpdateLocalHash();
		Packet->GetPayload()[0] = GetLocalHash();
	}

	inline void PrepareMetaPacket(const uint8_t subHeader, const uint8_t hash)
	{
		Packet->SetDefinition(SyncMetaDefinition);
		Packet->SetId(subHeader);
		Packet->GetPayload()[0] = hash;
	}

	//Writer
	void PrepareUpdateFinishedPacket()
	{
		PrepareMetaHashPacket(SYNC_META_SUB_HEADER_UPDATE_FINISHED);
	}

	bool PrepareBlockPacketHeader(const uint8_t index)
	{
		if (index < TrackedSurface->GetBlockCount())
		{
			Packet->SetDefinition(DataPacketDefinition);
			Packet->SetId(index);
			return true;
		}
		else
		{
			return false;
		}
	}

	//Reader
	inline void PrepareServiceDiscoveryPacket()
	{
		PrepareMetaPacket(SYNC_META_SUB_HEADER_SERVICE_DISCOVERY, GetSurfaceId());
	}

	inline void PrepareUpdateFinishedReplyPacket()
	{
		PrepareMetaHashPacket(SYNC_META_SUB_HEADER_UPDATE_FINISHED_REPLY);
	}

	inline void PrepareInvalidateRequestPacket()
	{
		PrepareMetaHashPacket(SYNC_META_SUB_HEADER_INVALIDATE_REQUEST);
	}
};
#endif