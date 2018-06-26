// SyncSurfaceBase.h

#ifndef _SYNCSURFACEBASE_h
#define _SYNCSURFACEBASE_h


#include <Arduino.h>
#include <Services\SyncSurface\SyncSurfaceBlock.h>
#include <Services\SyncSurface\SyncPacketDefinitions.h>
#include <Packet\LoLaPacket.h>
#include <Packet\PacketDefinition.h>
#include <Packet\LoLaPacketMap.h>


#define LOLA_SYNC_SURFACE_SERVICE_UPDATE_PERIOD_MILLIS			100
#define LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS				500

#define LOLA_SYNC_FAST_MAX_BLOCK_COUNT							PACKET_DEFINITION_SYNC_STATUS_PAYLOAD_SIZE

#define SYNC_SURFACE_STATUS_SUB_HEADER_FINISHED_SYNCED			1

#define SYNC_SURFACE_PROTOCOL_SUB_HEADER_REQUEST_SYNC			0
#define SYNC_SURFACE_PROTOCOL_SUB_HEADER_REQUEST_DISABLE		1
#define SYNC_SURFACE_PROTOCOL_SUB_HEADER_STARTING_SYNC			2
#define SYNC_SURFACE_PROTOCOL_SUB_HEADER_FINISHING_SYNC			3
#define SYNC_SURFACE_PROTOCOL_SUB_HEADER_DOUBLE_CHECK			4


class SyncSurfaceBase : public SyncSurfaceBlock
{
public:
	SyncSurfaceBase(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedSurfaceNotify* trackedSurface)
		: SyncSurfaceBlock(scheduler, LOLA_SYNC_SURFACE_SERVICE_UPDATE_PERIOD_MILLIS, loLa, trackedSurface)
	{
		SyncReportDefinition.SetBaseHeader(baseHeader);
		DataPacketDefinition.SetBaseHeader(baseHeader);
		ProtocolPacketDefinition.SetBaseHeader(baseHeader);
	}

private:
	SyncReportPacketDefinition SyncReportDefinition;
	SyncDataPacketDefinition DataPacketDefinition;
	SyncProtocolPacketDefinition ProtocolPacketDefinition;

	uint32_t LastDoubleCheckSentMillis = ILOLA_INVALID_MILLIS;

protected:
	uint8_t SyncTryCount = 0;

protected:
	virtual void OnBlockReceived(const uint8_t index, uint8_t * payload) {}

	//Reader
	virtual void OnSyncFinishingReceived() {}
	virtual void OnSyncStartingReceived() {}

	//Writer
	virtual void OnSyncReportReceived() {}
	virtual void OnSyncStartRequestReceived() {}
	virtual void OnSyncDisableRequestReceived() {}

	//Common
	virtual void OnDoubleCheckReceived() {}

protected:
	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		if (!packetMap->AddMapping(&SyncReportDefinition) ||
			!packetMap->AddMapping(&DataPacketDefinition) ||
			!packetMap->AddMapping(&ProtocolPacketDefinition))
		{
			return false;
		}

		return true;
	}
	
	virtual void OnStateUpdated(const SyncStateEnum newState)
	{
		SyncSurfaceBlock::OnStateUpdated(newState);
		switch (newState)
		{
		case SyncStateEnum::Starting:
			SyncTryCount = 0;
			break;
		case SyncStateEnum::FullSync:
		case SyncStateEnum::Synced:
		case SyncStateEnum::Resync:
		case SyncStateEnum::WaitingForTrigger:
		case SyncStateEnum::Disabled:
		default:
			break;
		}
	}

	void OnSyncedService()
	{
		if (TrackedSurface->GetTracker()->HasPending())
		{
			UpdateState(SyncStateEnum::Resync);
		}
		else if (!HashesMatch())
		{
			UpdateState(SyncStateEnum::Starting);
		}
		else if (LastDoubleCheckSentMillis == ILOLA_INVALID_MILLIS || Millis() - LastDoubleCheckSentMillis > ABSTRACT_SURFACE_SYNC_KEEP_ALIVE_MILLIS)
		{
			LastDoubleCheckSentMillis = Millis();
			
			PrepareDoubleCheckProtocolPacket();
			RequestSendPacket();
		}		
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		if (!IsSetupOk())
		{
			return false;
		}

		//Todo: Filter Should process packet

		if (header == DataPacketDefinition.GetHeader())
		{
			OnBlockReceived(incomingPacket->GetId(), incomingPacket->GetPayload());//To Reader.
			return true;
		}
		else if (header == ProtocolPacketDefinition.GetHeader())
		{
#ifdef DEBUG_LOLA
			Serial.print(Millis());
			Serial.print(F(": Received Protocol: "));
			Serial.println(incomingPacket->GetId());
#endif
			SetRemoteHash(incomingPacket->GetPayload()[0]);
			switch (incomingPacket->GetId())
			{			
			//To Writer.
			case SYNC_SURFACE_PROTOCOL_SUB_HEADER_REQUEST_SYNC:
				OnSyncStartRequestReceived();
				break;
			case SYNC_SURFACE_PROTOCOL_SUB_HEADER_REQUEST_DISABLE:
				OnSyncDisableRequestReceived();
				break;			

			//To Reader.
			case SYNC_SURFACE_PROTOCOL_SUB_HEADER_FINISHING_SYNC:
				SetRemoteHash(incomingPacket->GetPayload()[0]);
				OnSyncFinishingReceived();
				break;		
			case SYNC_SURFACE_PROTOCOL_SUB_HEADER_STARTING_SYNC:
				OnSyncStartingReceived();
				break;

			//To both.
			case SYNC_SURFACE_PROTOCOL_SUB_HEADER_DOUBLE_CHECK:
				SetRemoteHash(incomingPacket->GetPayload()[0]);
				OnDoubleCheckReceived();
				break;
			default:
				break;
			}
		}
		else if (header == SyncReportDefinition.GetHeader())
		{
			SetRemoteHash(incomingPacket->GetId());
			//To Writer.
			OnSyncReportReceived();
			return true;
		}

		return false;
	}

	void PrepareTrackerStatusPayload()
	{
		for (uint8_t i = 0; i < min(TrackedSurface->GetTracker()->GetBitCount(), LOLA_SYNC_FAST_MAX_BLOCK_COUNT); i++)
		{
			if (i < TrackedSurface->GetTracker()->GetSize())
			{
				Packet->GetPayload()[i] = TrackedSurface->GetTracker()->GetRawBlock(i);
			}
			else
			{
				Packet->GetPayload()[i] = 0; //Unused value in this packet.
			}
		}
	}

	void PrepareReportPacketHeader()
	{
		Packet->SetDefinition(&SyncReportDefinition);
		Packet->SetId(GetLocalHash());
	}

	void PrepareProtocolPacket(const uint8_t id)
	{
		Packet->SetDefinition(&ProtocolPacketDefinition);
		Packet->SetId(id);
		Packet->GetPayload()[0] = GetLocalHash();
	}

	void PrepareDoubleCheckProtocolPacket()
	{
		PrepareProtocolPacket(SYNC_SURFACE_PROTOCOL_SUB_HEADER_DOUBLE_CHECK);
	}

	bool PrepareBlockPacketHeader(const uint8_t index)
	{
		if (index < TrackedSurface->GetSize())
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