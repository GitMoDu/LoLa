// SyncSurfaceBase.h

#ifndef _SYNCSURFACEBASE_h
#define _SYNCSURFACEBASE_h

#include <LoLaDefinitions.h>
#include <Services\SyncSurface\AbstractSync.h>
#include <Packet\LoLaPacket.h>
#include <Packet\PacketDefinition.h>
#include <Packet\LoLaPacketMap.h>


class SyncSurfaceBase : public AbstractSync
{
private:
	static const uint8_t SYNC_META_SUB_HEADER_SERVICE_DISCOVERY = 0;
	static const uint8_t SYNC_META_SUB_HEADER_UPDATE_FINISHED = 1;
	static const uint8_t SYNC_META_SUB_HEADER_UPDATE_FINISHED_REPLY = 2;
	static const uint8_t SYNC_META_SUB_HEADER_INVALIDATE_REQUEST = 3;


protected:
	PacketDefinition* MetaDefinition = nullptr;
	PacketDefinition* DataDefinition = nullptr;


	uint8_t* SurfaceData = nullptr;

	const uint32_t ThrottlePeriodMillis = 0;


public:
	SyncSurfaceBase(Scheduler* scheduler, LoLaPacketDriver* driver, ITrackedSurface* trackedSurface, const uint32_t throttlePeriodMillis)
		: AbstractSync(scheduler, FAST_CHECK_PERIOD_MILLIS, driver, trackedSurface)
		, SurfaceData(trackedSurface->SurfaceData)
		, ThrottlePeriodMillis(throttlePeriodMillis)
	{
	}

	bool Setup()
	{
		if (!AbstractSync::Setup())
		{
			return false;
		}

		if (!LoLaDriver->PacketMap.RegisterMapping(MetaDefinition) ||
			!LoLaDriver->PacketMap.RegisterMapping(DataDefinition))
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
		if ((millis() - LastSyncMillis > ThrottlePeriodMillis)
			|| LastSyncMillis == 0)
		{

			return true;
		}
		else
		{
			return false;
		}
	}
	bool OnPacketReceived(const uint8_t header, const uint32_t timestamp, uint8_t* payload)
	{
		if (header == DataDefinition->Header)
		{
			//To Reader.
			if (SyncState != SyncStateEnum::Disabled)
			{
				SetRemoteHash(payload[1]);
				OnBlockReceived(payload[0], &payload[1 + 1]);
			}

			return true;
		}
		else if (header == MetaDefinition->Header)
		{
			if (SyncState != SyncStateEnum::Disabled)
			{
				switch (payload[0])
				{
					//To Reader.
				case SYNC_META_SUB_HEADER_UPDATE_FINISHED:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
					Serial.println(F("OnUpdateFinishedReceived"));
#endif
					SetRemoteHash(payload[1]);
					OnUpdateFinishedReceived();
					break;

					//To Writer.
				case SYNC_META_SUB_HEADER_UPDATE_FINISHED_REPLY:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
					Serial.println(F("OnUpdateFinishedReplyReceived"));
#endif
					SetRemoteHash(payload[1]);
					OnUpdateFinishedReplyReceived();
					break;
				case SYNC_META_SUB_HEADER_INVALIDATE_REQUEST:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
					Serial.println(F("OnInvalidateReceived"));
#endif
					SetRemoteHash(payload[1]);
					OnInvalidateRequestReceived();
					break;
				case SYNC_META_SUB_HEADER_SERVICE_DISCOVERY:
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
					Serial.println(F("OnServiceDiscoveryReceived"));
#endif
					OnServiceDiscoveryReceived(payload[1]);
					break;
				default:
					break;
				}
			}

			return true;
		}

		return false;
	}

	void SendMetaHashPacket(const uint8_t subHeader)
	{
		SendMetaPacket(subHeader, GetLocalHash());
	}

	void SendMetaPacket(const uint8_t subHeader, const uint8_t value)
	{
		GetOutPayload()[0] = subHeader;
		GetOutPayload()[1] = value;

		RequestSendPacket(MetaDefinition->Header);
	}
};
#endif