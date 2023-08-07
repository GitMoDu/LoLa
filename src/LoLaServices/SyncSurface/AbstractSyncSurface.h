// AbstractSyncSurface.h

#ifndef _ABSTRACT_SYNC_SURFACE_h
#define _ABSTRACT_SYNC_SURFACE_h


#include "..\..\LoLaServices\AbstractLoLaDiscoveryService.h"
#include <ITrackedSurface.h>
#include <FastCRC.h>
#include "SyncSurfaceDefinitions.h"


template<const uint8_t Port,
	const uint8_t MaxSendPayloadSize = 3,
	const uint32_t NoDiscoveryTimeOut = 30000>
class AbstractSyncSurface
	: public AbstractLoLaDiscoveryService<Port, MaxSendPayloadSize, NoDiscoveryTimeOut>
{
private:
	using BaseClass = AbstractLoLaDiscoveryService<Port, MaxSendPayloadSize, NoDiscoveryTimeOut>;

protected:
	using BaseClass::OutPacket;

protected:
	using BaseClass::RequestSendPacket;
	using BaseClass::ResetPacketThrottle;
	using BaseClass::PacketThrottle;

	static const uint32_t FAST_CHECK_PERIOD_MILLIS = 1;

	static const uint32_t SLOW_CHECK_PERIOD_MILLIS = 10;

	static const uint32_t UPDATE_BACK_OFF_PERIOD_MILLIS = 5;
	static const uint32_t SYNC_CONFIRM_RESEND_PERIOD_MILLIS = 30;
	// 
	//static const uint32_t SEND_FAILED_RETRY_PERIOD = 100;
	//static const uint32_t RECEIVE_FAILED_PERIOD = SEND_FAILED_RETRY_PERIOD * 4;

private:


	uint8_t LastRemoteHash = 0;
	bool RemoteHashIsSet = false;

	//CRC Calculator.
	FastCRC8 CRC8;

	uint8_t LastLocalHash = 0;
	bool LocalHashIsUpToDate = false;

	uint32_t StateStartTime = 0;


protected:
	bool Synced = false;

	ITrackedSurface& TrackedSurface;

	uint32_t LastSync = 0;
	uint8_t* SurfaceData;

protected:
	virtual void OnStateUpdated(const bool synced) {}

public:
	AbstractSyncSurface(Scheduler& scheduler, ILoLaLink* link, ITrackedSurface& trackedSurface)
		: BaseClass(scheduler, link)
		, CRC8()
		, TrackedSurface(trackedSurface)
		, SurfaceData(trackedSurface.GetRawData())
	{
	}

	virtual const bool Setup()
	{
		return BaseClass::Setup();
	}

	const bool IsSynced()
	{
		return Synced;
	}

public:
	virtual void OnServiceStarted()
	{
		UpdateSyncState(false);
	}

	virtual void OnServiceEnded()
	{
		// Nothing to do, service callback won't run.
	}

protected:
	const bool RequestSendMetaPacket(const uint8_t header)
	{
		OutPacket.SetPort(Port);
		OutPacket.SetHeader(header);
		OutPacket.Payload[SyncCommonDefinition::CRC_OFFSET] = GetLocalHash();

		return RequestSendPacket(SyncCommonDefinition::PAYLOAD_SIZE);
	}

	const uint8_t GetLocalHash()
	{
		if (!LocalHashIsUpToDate)
		{
			LocalHashIsUpToDate = true;
			LastLocalHash = CRC8.smbus(TrackedSurface.GetRawData(), TrackedSurface.GetTotalSize());
		}

		return LastLocalHash;
	}

	void InvalidateLocalHash()
	{
		LocalHashIsUpToDate = false;
	}

	void ForceUpdateLocalHash()
	{
		LocalHashIsUpToDate = false;
		GetLocalHash();
	}

	const bool HasRemoteHash()
	{
		return RemoteHashIsSet;
	}

	void SetRemoteHash(const uint8_t remoteHash)
	{
		RemoteHashIsSet = true;
		LastRemoteHash = remoteHash;
	}

	void InvalidateRemoteHash()
	{
		RemoteHashIsSet = false;
	}

	const bool HashesMatch()
	{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
		if (!HasRemoteHash())
		{
			Serial.println(F("No Remote hash"));
		}
		else if (GetLocalHash() != LastRemoteHash)
		{
			Serial.println(F("Hash mismatch"));
		}
#endif
		return (HasRemoteHash() && GetLocalHash() == LastRemoteHash);
	}

	const uint32_t GetElapsedSinceStateStart()
	{
		return millis() - StateStartTime;
	}

	void UpdateSyncState(const bool newState)
	{
		if (Synced != newState)
		{
			StateStartTime = millis();
			ResetPacketThrottle();

			if (newState)
			{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Synced"));
#endif
				LastSync = StateStartTime;
			}
			else
			{
#if defined(DEBUG_LOLA) && defined(LOLA_SYNC_FULL_DEBUG)
				Serial.println(F("Syncing"));
#endif
				LastSync = 0;
				InvalidateLocalHash();
				Synced = newState;

				OnStateUpdated(Synced);
			}
		}
	}

protected:
	//TODO: replace with template constant
	virtual const uint8_t GetServiceId() final
	{
		return TrackedSurface.GetId();
	}

	//virtual void OnDiscoveredSendRequestFail(const uint8_t header)
	//{
	//	////In case the send fails, this prevents from immediate resending.
	//	//SetNextRunDelay(SEND_FAILED_RETRY_PERIOD);
	//}
};
#endif