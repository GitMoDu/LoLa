// SyncSurfaceBase.h

#ifndef _SYNCSURFACEBASE_h
#define _SYNCSURFACEBASE_h


#include <Arduino.h>
#include <Services\SyncSurface\SyncSurfaceBlock.h>
#include <Services\SyncSurface\SyncPacketDefinitions.h>
#include <Packet\LoLaPacket.h>
#include <Packet\PacketDefinition.h>
#include <Packet\LoLaPacketMap.h>


#define LOLA_SYNC_SURFACE_SERVICE_UPDATE_PERIOD_MILLIS 100

//#define LOLA_SYNC_SURFACE_SERVICE_START_DELAY_DURATION_MILLIS 3000
#define LOLA_SYNC_SURFACE_SERVICE_INVALIDATE_PERIOD_MILLIS 2000
#define LOLA_SYNC_SURFACE_BACK_OFF_DURATION_MILLIS 200
#define LOLA_SYNC_SURFACE_MAX_UNRESPONSIVE_REMOTE_DURATION_MILLIS 10000//60000

#define LOLA_SYNC_KEEP_ALIVE_MILLIS (LOLA_SYNC_SURFACE_SERVICE_INVALIDATE_PERIOD_MILLIS/2)

#define SYNC_SESSION_ID_STATUS_OFFSET_READER_START_SYNC 0
#define SYNC_SESSION_ID_STATUS_OFFSET_READER_SYNC_ENSURE 1
#define SYNC_SESSION_ID_STATUS_OFFSET_WRITER_STARTING_SYNC 2
#define SYNC_SESSION_ID_STATUS_OFFSET_WRITER_ALL_DATA_UPDATED 3

#define SYNC_SESSION_ID_STATUS_OFFSET_MAX SYNC_SESSION_ID_STATUS_OFFSET_WRITER_ALL_DATA_UPDATED



class SyncSurfaceBase : public SyncSurfaceBlock
{
public:
	SyncSurfaceBase(Scheduler* scheduler, ILoLa* loLa, const uint8_t baseHeader, ITrackedSurfaceNotify* trackedSurface)
		: SyncSurfaceBlock(scheduler, LOLA_SYNC_SURFACE_SERVICE_UPDATE_PERIOD_MILLIS, loLa, trackedSurface)
	{
		SyncStatusDefinition.SetBaseHeader(baseHeader);
		DataPacketDefinition.SetBaseHeader(baseHeader);
	}

private:
	SyncStatusPacketDefinition SyncStatusDefinition;
	SyncDataPacketDefinition DataPacketDefinition;

	uint8_t SyncSessionId = 0;
	bool SyncSessionHasSynced = false;

	uint8_t LastRemoteHash = 0;
	uint32_t LastRemoteHashReceived = 0;

protected:
	enum SyncStatusCodeEnum : uint8_t
	{
		Reader_StartSync = 0,
		Writer_StartingSync = 1,
		Writer_AllDataUpdated = 2,
		Reader_SyncEnsure = 3,
		Reader_Disable = 4
	};

	uint8_t SyncTryCount = 0;
	uint32_t SyncStartMillis = 0;

protected:
	virtual void OnBlockReceived(const uint8_t index, uint8_t * payload) {}

	virtual void OnWriterAllDataUpdatedReceived() {}
	virtual void OnWriterStartingSyncReceived() {}
	virtual void OnReaderSyncEnsureReceived() {}
	virtual void OnReaderStartSyncReceived() {}
	virtual void OnReaderDisable() {}

	virtual void OnWriterAllDataUpdatedAck() {}
	virtual void OnWriterStartingSyncAck() {}
	virtual void OnReaderSyncEnsureAck() {}
	virtual void OnReaderStartSyncAck() {}

	virtual void OnSyncFailed() {}

protected:
	virtual bool OnSetup()
	{
		if (SyncSurfaceBlock::OnSetup())
		{
			GetSurface()->Initialize();
			return true;
		}

		return false;
	}

protected:
	bool SessionHasSynced()
	{
		return SyncSessionHasSynced;
	}

	void StampSessionSynced()
	{
		//if (!SyncSessionHasSynced)
		//{

		//}
		SyncSessionHasSynced = true;
	}

	virtual bool OnSyncStart()
	{
		InvalidateHash();
		LastRemoteHash = 0;
		LastRemoteHashReceived = 0;
		SyncTryCount = 0;
		TrackedSurface->GetTracker()->SetAllPending();
		ResetSyncSession();

		return true;
	}

	bool ProcessPacket(ILoLaPacket* incomingPacket, const uint8_t header)
	{
		if (!IsSetupOk())
		{
			return false;
		}

		if (header == DataPacketDefinition.GetHeader())
		{
			TimeStampReceived(Millis());
			OnBlockReceived(incomingPacket->GetId(), incomingPacket->GetPayload());
			return true;
		}
		else if (header == SyncStatusDefinition.GetHeader())
		{
			TimeStampReceived(Millis());
			//Validate session Id before calling methods.
			switch ((SyncStatusCodeEnum)incomingPacket->GetPayload()[0])
			{
			case SyncStatusCodeEnum::Reader_StartSync:
				//Writer receives the session Id when sync start is requested.
				SyncSessionId = incomingPacket->GetId() - SYNC_SESSION_ID_STATUS_OFFSET_READER_START_SYNC;
				OnReaderStartSyncReceived();
				break;
			case SyncStatusCodeEnum::Reader_SyncEnsure:
				//Writer double checks the session Id, before accepting the packet.
				if (incomingPacket->GetId() == SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_READER_SYNC_ENSURE)
				{
					LastRemoteHashReceived = LastReceived;
					LastRemoteHash = incomingPacket->GetPayload()[1];
					OnReaderSyncEnsureReceived();
				}
				break;
			case SyncStatusCodeEnum::Writer_StartingSync:
				//Writer double checks the session Id, before accepting the packet.
				if (incomingPacket->GetId() == SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_WRITER_STARTING_SYNC)
				{
					OnWriterStartingSyncReceived();
				}
				break;
			case SyncStatusCodeEnum::Writer_AllDataUpdated:
				//Writer double checks the session Id, before accepting the packet.
				if (incomingPacket->GetId() == SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_WRITER_ALL_DATA_UPDATED)
				{
					LastRemoteHashReceived = LastReceived;
					LastRemoteHash = incomingPacket->GetPayload()[1];
					OnWriterAllDataUpdatedReceived();
				}
				break;
			case SyncStatusCodeEnum::Reader_Disable:
				OnReaderDisable();
				break;
			default:
				break;
			}

			return true;
		}

		return false;
	}

	bool ProcessAck(const uint8_t header, const uint8_t id)
	{
		if (!IsSetupOk())
		{
			return false;
		}

		if (header == DataPacketDefinition.GetHeader())
		{
			//No Ack, just swallow.
			return true;
		}
		else if (header == SyncStatusDefinition.GetHeader())
		{
			TimeStampReceived(Millis());

			if (id == SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_READER_START_SYNC)
			{
				OnReaderStartSyncAck();
			}
			else if (id == SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_READER_SYNC_ENSURE)
			{
				OnReaderSyncEnsureAck();
			}
			else if (id == SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_WRITER_STARTING_SYNC)
			{
				OnWriterStartingSyncAck();
			}
			else if (id == SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_WRITER_ALL_DATA_UPDATED)
			{
				OnWriterAllDataUpdatedAck();
			}

			return true;
		}

		return false;
	}

	bool IsRemoteHashValid()
	{
		return ((LastRemoteHashReceived != 0) &&
			GetHash() == LastRemoteHash);
	}

	void ResetSyncSession()
	{
		uint8_t NewSessionId = SyncSessionId;
		uint8_t TryCount = 0;
		while (NewSessionId == SyncSessionId)
		{
			TryCount++;
			if (TryCount > 3)
			{
				NewSessionId = SyncSessionId++;
			}
			else
			{
				NewSessionId = random(0xFF);
			}
		}
		SyncSessionId = NewSessionId;
		SyncSessionHasSynced = false;
	}

	void PrepareReaderStartSyncPacket()
	{
		Packet->SetDefinition(&SyncStatusDefinition);
		Packet->GetPayload()[0] = SyncStatusCodeEnum::Reader_StartSync;
		Packet->GetPayload()[1] = 0; //Unused value in this packet.
		Packet->SetId(SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_READER_START_SYNC); //Value is tracked by Ack.
	}

	void PrepareReaderEnsureSyncPacket()
	{
		Packet->SetDefinition(&SyncStatusDefinition);
		Packet->GetPayload()[0] = SyncStatusCodeEnum::Reader_SyncEnsure;
		Packet->GetPayload()[1] = GetHash();
		Packet->SetId(SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_READER_START_SYNC); //Value is tracked by Ack.
	}

	void PrepareWriterStartingSyncPacket()
	{
		Packet->SetDefinition(&SyncStatusDefinition);
		Packet->GetPayload()[0] = SyncStatusCodeEnum::Writer_StartingSync;
		Packet->GetPayload()[1] = 0; //Unused value in this packet.
		Packet->SetId(SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_WRITER_STARTING_SYNC); //Value is tracked by Ack.
	}

	void PrepareWriterAllDataUpdated()
	{
		Packet->SetDefinition(&SyncStatusDefinition);
		Packet->GetPayload()[0] = SyncStatusCodeEnum::Writer_AllDataUpdated;
		Packet->GetPayload()[1] = GetHash();
		Packet->SetId(SyncSessionId + SYNC_SESSION_ID_STATUS_OFFSET_WRITER_ALL_DATA_UPDATED); //Value is tracked by Ack.
	}

	bool PrepareBlockPacket(const uint8_t index)
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

	bool OnAddPacketMap(LoLaPacketMap* packetMap)
	{
		if (!packetMap->AddMapping(&SyncStatusDefinition) ||
			!packetMap->AddMapping(&DataPacketDefinition))
		{
			return false;
		}

		return true;
	}
};

#endif

