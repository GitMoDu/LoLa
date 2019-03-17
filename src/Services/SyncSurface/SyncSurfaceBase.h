// SyncSurfaceBase.h

#ifndef _SYNCSURFACEBASE_h
#define _SYNCSURFACEBASE_h

#include <LoLaDefinitions.h>
#include <Services\SyncSurface\AbstractSync.h>
#include <Services\SyncSurface\SyncPacketDefinitions.h>
#include <Packet\LoLaPacket.h>
#include <Packet\PacketDefinition.h>
#include <Packet\LoLaPacketMap.h>

#define SYNC_SURFACE_META_SUB_HEADER_SERVICE_DISCOVERY		0
#define SYNC_SURFACE_META_SUB_HEADER_UPDATE_FINISHED		1

#define SYNC_SURFACE_META_SUB_HEADER_UPDATE_FINISHED_REPLY	2
#define SYNC_SURFACE_META_SUB_HEADER_INVALIDATE_REQUEST		3

class SyncSurfaceBase : public AbstractSync
{
private:
	SyncAbstractPacketDefinition* SyncMetaDefinition = nullptr;
	SyncAbstractPacketDefinition* DataPacketDefinition = nullptr;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} ATUI;

	TemplateLoLaPacket<LOLA_PACKET_MIN_PACKET_SIZE + PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE> PacketHolder;

public:
	SyncSurfaceBase(Scheduler* scheduler, ILoLaDriver* driver, ITrackedSurface* trackedSurface,
		SyncAbstractPacketDefinition* metaDefinition, SyncAbstractPacketDefinition* dataDefinition)
		: AbstractSync(scheduler, ABSTRACT_SURFACE_FAST_CHECK_PERIOD_MILLIS, driver, trackedSurface, &PacketHolder)
	{
		SyncMetaDefinition = metaDefinition;
		DataPacketDefinition = dataDefinition;
#ifdef DEBUG_LOLA
		SyncMetaDefinition->SetOwner(trackedSurface);
		DataPacketDefinition->SetOwner(trackedSurface);		
#endif
	}

protected:
	//Reader
	virtual void OnBlockReceived(const uint8_t index, uint8_t * payload) {}
	virtual void OnUpdateFinishedReceived() {}
	virtual void OnSyncFinishedReceived() {}

	//Writer
	virtual void OnServiceDiscoveryReceived() {}
	virtual void OnUpdateFinishedReplyReceived() {}
	virtual void OnInvalidateRequestReceived() {}

protected:
	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		if (!packetMap->AddMapping(SyncMetaDefinition) ||
			!packetMap->AddMapping(DataPacketDefinition))
		{
			return false;
		}

		return true;
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket)
	{
		if (incomingPacket->GetDataHeader() == DataPacketDefinition->GetHeader())
		{
			//To Reader.
			OnBlockReceived(incomingPacket->GetId(), incomingPacket->GetPayload());

			return true;
		}
		else if (incomingPacket->GetDataHeader() == SyncMetaDefinition->GetHeader())
		{
			SetRemoteHash(incomingPacket->GetPayload()[0]);

			switch (incomingPacket->GetId())
			{
				//To Reader.
			case SYNC_SURFACE_META_SUB_HEADER_UPDATE_FINISHED:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("OnUpdateFinishedReceived"));
#endif
				OnUpdateFinishedReceived();
				break;

				//To Writer.
			case SYNC_SURFACE_META_SUB_HEADER_UPDATE_FINISHED_REPLY:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("OnUpdateFinishedReplyReceived"));
#endif
				OnUpdateFinishedReplyReceived();
				break;
			case SYNC_SURFACE_META_SUB_HEADER_INVALIDATE_REQUEST:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("OnInvalidateReceived"));
#endif
				OnInvalidateRequestReceived();
				break;
			case SYNC_SURFACE_META_SUB_HEADER_SERVICE_DISCOVERY:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("OnServiceDiscoveryReceived"));
#endif
				OnServiceDiscoveryReceived();
				break;
			default:
				break;
			}

			return true;
		}

		return false;
	}

	void UpdateBlockData(const uint8_t blockIndex, uint8_t * payload)
	{
		uint8_t IndexOffset = blockIndex * SYNC_SURFACE_BLOCK_SIZE;

		for (uint8_t i = 0; i < SYNC_SURFACE_BLOCK_SIZE; i++)
		{
			TrackedSurface->GetData()[IndexOffset + i] = payload[i];
		}
	}

	void PrepareBlockPacketPayload(const uint8_t index, uint8_t * payload)
	{
		uint8_t IndexOffset = index * SYNC_SURFACE_BLOCK_SIZE;

		for (uint8_t i = 0; i < SYNC_SURFACE_BLOCK_SIZE; i++)
		{
			payload[i] = TrackedSurface->GetData()[IndexOffset + i];
		}
	}

	void PrepareMetaPacket(const uint8_t subHeader)
	{
		Packet->SetDefinition(SyncMetaDefinition);
		Packet->SetId(subHeader);
		UpdateLocalHash();
		Packet->GetPayload()[0] = GetLocalHash();
	}

	//Writer
	void PrepareUpdateFinishedPacket()
	{
		PrepareMetaPacket(SYNC_SURFACE_META_SUB_HEADER_UPDATE_FINISHED);
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
	void PrepareServiceDiscoveryPacket()
	{
		PrepareMetaPacket(SYNC_SURFACE_META_SUB_HEADER_SERVICE_DISCOVERY);
	}

	void PrepareUpdateFinishedReplyPacket()
	{
		PrepareMetaPacket(SYNC_SURFACE_META_SUB_HEADER_UPDATE_FINISHED_REPLY);
	}

	void PrepareInvalidateRequestPacket()
	{
		PrepareMetaPacket(SYNC_SURFACE_META_SUB_HEADER_INVALIDATE_REQUEST);
	}
};
#endif