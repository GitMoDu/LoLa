// SyncSurfaceBase.h

#ifndef _SYNCSURFACEBASE_h
#define _SYNCSURFACEBASE_h


#include <Arduino.h>
#include <Services\SyncSurface\AbstractSync.h>
#include <Services\SyncSurface\SyncPacketDefinitions.h>
#include <Packet\LoLaPacket.h>
#include <Packet\PacketDefinition.h>
#include <Packet\LoLaPacketMap.h>


#define ABSTRACT_SURFACE_SYNC_SEND_BACK_OFF_PERIOD_MILLIS   (uint32_t)5
#define ABSTRACT_SURFACE_SYNC_FAST_CHECK_PERIOD_MILLIS		(uint32_t)1

#define SYNC_SURFACE_META_SUB_HEADER_SERVICE_DISCOVERY		0
#define SYNC_SURFACE_META_SUB_HEADER_SYNC_FINISHING			1
#define SYNC_SURFACE_META_SUB_HEADER_SYNC_FINISHED			2
#define SYNC_SURFACE_META_SUB_HEADER_FINISHING_RESPONSE		3

class SyncSurfaceBase : public AbstractSync
{
private:
	SyncMetaPacketDefinition SyncMetaDefinition;
	SyncDataPacketDefinition DataPacketDefinition;

	uint32_t LastReceived = ILOLA_INVALID_MILLIS;

	union ArrayToUint32 {
		byte array[4];
		uint32_t uint;
	} ATUI;

	TemplateLoLaPacket<LOLA_PACKET_MIN_WITH_ID_SIZE + PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE> PacketHolder;

public:
	SyncSurfaceBase(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedSurface* trackedSurface)
		: AbstractSync(scheduler, ABSTRACT_SURFACE_SYNC_FAST_CHECK_PERIOD_MILLIS, loLa, trackedSurface, &PacketHolder)
	{
		SyncMetaDefinition.SetBaseHeader(baseHeader);
		DataPacketDefinition.SetBaseHeader(baseHeader);
	}

protected:
	virtual void OnBlockReceived(const uint8_t index, uint8_t * payload) {}

	//Reader
	virtual void OnSyncFinishingReceived() {}
	virtual void OnSyncFinishedReceived() {}

	//Writer
	virtual void OnServiceDiscoveryReceived() {}
	virtual void OnSyncResponseReceived() {}

	//Common
	virtual void OnSyncStateUpdated(const SyncStateEnum newState) {}

protected:
	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		if (!packetMap->AddMapping(&SyncMetaDefinition) ||
			!packetMap->AddMapping(&DataPacketDefinition))
		{
			return false;
		}

		return true;
	}


	uint32_t GetElapsedSinceLastReceived()
	{
		if (LastReceived == ILOLA_INVALID_MILLIS)
		{
			return ILOLA_INVALID_MILLIS;
		}
		else
		{
			return Millis() - LastReceived;
		}
	}

	void OnStateUpdated(const SyncStateEnum newState)
	{
		AbstractSync::OnStateUpdated(newState);
		OnSyncStateUpdated(newState);
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		if (header == DataPacketDefinition.GetHeader())
		{
			LastReceived = Millis();
			//To Reader.
			OnBlockReceived(incomingPacket->GetId(), incomingPacket->GetPayload());

			return true;
		}
		else if (header == SyncMetaDefinition.GetHeader())
		{
			LastReceived = Millis();
			SetRemoteHash(incomingPacket->GetPayload()[0]);

			switch (incomingPacket->GetId())
			{
				//To Reader.
			case SYNC_SURFACE_META_SUB_HEADER_SYNC_FINISHING:
				OnSyncFinishingReceived();
				break;
			case SYNC_SURFACE_META_SUB_HEADER_SYNC_FINISHED:
				OnSyncFinishedReceived();
				break;

				//To Writer.
			case SYNC_SURFACE_META_SUB_HEADER_FINISHING_RESPONSE:
				OnSyncResponseReceived();
				break;
			case SYNC_SURFACE_META_SUB_HEADER_SERVICE_DISCOVERY:
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
		Packet->SetDefinition(&SyncMetaDefinition);
		Packet->SetId(subHeader);
		UpdateLocalHash();
		Packet->GetPayload()[0] = GetLocalHash();
	}

	void PrepareServiceDiscoveryPacket()
	{
		PrepareMetaPacket(SYNC_SURFACE_META_SUB_HEADER_SERVICE_DISCOVERY);
	}

	void PrepareFinishingSyncPacket()
	{
		PrepareMetaPacket(SYNC_SURFACE_META_SUB_HEADER_SYNC_FINISHING);
	}

	void PrepareFinishedSyncPacket()
	{
		PrepareMetaPacket(SYNC_SURFACE_META_SUB_HEADER_SYNC_FINISHED);
	}

	void PrepareFinishingResponsePacket()
	{
		PrepareMetaPacket(SYNC_SURFACE_META_SUB_HEADER_FINISHING_RESPONSE);
	}

	bool PrepareBlockPacketHeader(const uint8_t index)
	{
		if (index < TrackedSurface->GetBlockCount())
		{
			Packet->SetDefinition(&DataPacketDefinition);
			Packet->SetId(index);
			return true;
		}
		else
		{
			return false;
		}
	}
};
#endif