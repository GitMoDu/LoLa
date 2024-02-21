// AbstractSurfaceService.h

#ifndef _ABSTRACT_SURFACE_SERVICE_h
#define _ABSTRACT_SURFACE_SERVICE_h


/*
* https://github.com/RobTillaart/Fletcher
*/
#include <Fletcher16.h>

#include "..\..\LoLaServices\AbstractDiscovery\AbstractDiscoveryService.h"
#include "ISurface.h"
#include "SurfaceDefinitions.h"

using namespace SurfaceDefinitions;

template<const uint8_t Port,
	const uint32_t ServiceId,
	const uint8_t MaxSendPayloadSize>
class AbstractSurfaceService
	: public AbstractLoLaDiscoveryService<Port, ServiceId, MaxSendPayloadSize>
{
private:
	using BaseClass = AbstractLoLaDiscoveryService<Port, ServiceId, MaxSendPayloadSize>;
	using SyncMetaDefinition = SyncDefinition<0>;

protected:
	using BaseClass::OutPacket;
	using BaseClass::RequestSendPacket;
	using BaseClass::ResetPacketThrottle;
	using BaseClass::PacketThrottle;

private:
	Fletcher16 Crc{};

protected:
	ISurface* TrackedSurface;
private:
	uint16_t LastRemoteHash = 0;
protected:
	uint8_t SurfaceSize = 0;
private:
	bool RemoteHashIsSet = false;

public:
	AbstractSurfaceService(Scheduler& scheduler, ILoLaLink* link, ISurface* surface)
		: BaseClass(scheduler, link)
		, TrackedSurface(surface)
	{}

	virtual const bool Setup()
	{
		if (TrackedSurface != nullptr
			&& TrackedSurface->GetBlockData() != nullptr)
		{
			SurfaceSize = ISurface::GetByteCount(TrackedSurface->GetBlockCount());
		}
		else
		{
			SurfaceSize = 0;
		}

		return BaseClass::Setup() && SurfaceSize > 0;
	}

public:
	virtual void OnServiceStarted()
	{
		if (SurfaceSize > 0)
		{
			InvalidateRemoteHash();
			ResetPacketThrottle();
			TrackedSurface->SetAllBlocksPending();
			TrackedSurface->SetHot(false);
			Task::enable();
		}
	}

	virtual void OnServiceEnded()
	{
		if (SurfaceSize > 0)
		{
			TrackedSurface->SetHot(false);
			TrackedSurface->NotifyUpdated();
			InvalidateRemoteHash();
			Task::disable();
		}
	}

protected:
	const bool RequestSendMeta(const uint8_t header)
	{
		return RequestSendMeta(GetLocalHash(), header);
	}

	const bool RequestSendMeta(const uint16_t hash, const uint8_t header)
	{
		OutPacket.SetPort(Port);
		OutPacket.SetHeader(header);
		OutPacket.Payload[SyncMetaDefinition::CRC_OFFSET] = hash;
		OutPacket.Payload[SyncMetaDefinition::CRC_OFFSET + 1] = hash >> 8;

		return RequestSendPacket(SyncMetaDefinition::PAYLOAD_SIZE);
	}

	const uint16_t GetLocalHash()
	{
		Crc.begin();
		Crc.add(TrackedSurface->GetBlockData(), SurfaceSize);

		return Crc.getFletcher();
	}

	const bool HasRemoteHash()
	{
		return RemoteHashIsSet;
	}

	void SetRemoteHashArray(const uint8_t* remoteHashArray)
	{
		LastRemoteHash = ((uint16_t)remoteHashArray[0]) | ((uint16_t)remoteHashArray[1] << 8);
		RemoteHashIsSet = true;
	}

	void InvalidateRemoteHash()
	{
		RemoteHashIsSet = false;
	}

	const bool HashesMatch(const uint16_t hash)
	{
		return (HasRemoteHash() && hash == LastRemoteHash);
	}

	const bool HashesMatch()
	{
		return (HasRemoteHash() && GetLocalHash() == LastRemoteHash);
	}
};
#endif