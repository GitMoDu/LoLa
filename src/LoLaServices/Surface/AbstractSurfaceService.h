// AbstractSurfaceService.h

#ifndef _ABSTRACT_SURFACE_SERVICE_h
#define _ABSTRACT_SURFACE_SERVICE_h

/*
* https://github.com/RobTillaart/Fletcher
*/
#include <Fletcher16.h>

#include "..\..\LoLaServices\Discovery\AbstractDiscoveryService.h"
#include "ISurface.h"
#include "SurfaceDefinitions.h"

using namespace SurfaceDefinitions;

/// <summary>
/// Base for Surface services.
/// </summary>
/// <typeparam name="Port">The port registered for this service.</typeparam>
/// <typeparam name="ServiceId">Unique service identifier.</typeparam>
/// <typeparam name="MaxSendPayloadSize">The max packet payload sent by this service.</typeparam>
template<const uint8_t Port,
	const uint32_t ServiceId,
	const uint8_t MaxSendPayloadSize>
class AbstractSurfaceService
	: public AbstractDiscoveryService<Port, ServiceId, MaxSendPayloadSize>
{
private:
	using BaseClass = AbstractDiscoveryService<Port, ServiceId, MaxSendPayloadSize>;
	using SyncMetaDefinition = SyncDefinition<0>;

protected:
	using BaseClass::OutPacket;
	using BaseClass::CanRequestSend;
	using BaseClass::RequestSendPacket;
	using BaseClass::ResetPacketThrottle;
	using BaseClass::PacketThrottle;

private:
	Fletcher16 Crc{};

protected:
	ISurface* Surface;
	uint8_t* BlockData = nullptr;

private:
	uint16_t RemoteHash = 0;

protected:
	uint8_t SurfaceSize = 0;

private:
	bool RemoteHashIsSet = false;

public:
	AbstractSurfaceService(Scheduler& scheduler, ILoLaLink* link, ISurface* surface)
		: BaseClass(scheduler, link)
		, Surface(surface)
	{}

	virtual const bool Setup()
	{
		if (Surface != nullptr)
		{
			BlockData = Surface->GetBlockData();
			SurfaceSize = ISurface::GetByteCount(Surface->GetBlockCount());

			if (BlockData != nullptr && SurfaceSize > 0)
			{
				return BaseClass::Setup();
			}
		}

		BlockData = nullptr;
		SurfaceSize = 0;

		return false;
	}

protected:
	virtual void OnServiceStarted()
	{
		if (SurfaceSize > 0 && BlockData != nullptr)
		{
			Task::enableDelayed(0);
			InvalidateRemoteHash();
			ResetPacketThrottle();
			Surface->SetAllBlocksPending();
		}
		else
		{
			Task::disable();
		}
	}

	virtual void OnServiceEnded()
	{
		if (SurfaceSize > 0)
		{
			Surface->SetHot(false);
			Surface->NotifyUpdated();
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
		if (CanRequestSend())
		{
			OutPacket.SetPort(Port);
			OutPacket.SetHeader(header);
			OutPacket.Payload[SyncMetaDefinition::CRC_OFFSET] = hash;
			OutPacket.Payload[SyncMetaDefinition::CRC_OFFSET + 1] = hash >> 8;

			return RequestSendPacket(SyncMetaDefinition::PAYLOAD_SIZE);
		}

		return RequestSendPacket(SyncMetaDefinition::PAYLOAD_SIZE);
	}

	const uint16_t GetLocalHash()
	{
		Crc.begin();
		Crc.add(Surface->GetBlockData(), SurfaceSize);

		return Crc.getFletcher();
	}

	const bool HasRemoteHash()
	{
		return RemoteHashIsSet;
	}

	void SetRemoteHashArray(const uint8_t* remoteHashArray)
	{
		RemoteHash = ((uint16_t)remoteHashArray[0]) | ((uint16_t)remoteHashArray[1] << 8);
		RemoteHashIsSet = true;
	}

	void InvalidateRemoteHash()
	{
		RemoteHashIsSet = false;
	}

	const bool HashesMatch(const uint16_t hash)
	{
		return (HasRemoteHash() && hash == RemoteHash);
	}

	const bool HashesMatch()
	{
		return (HasRemoteHash() && GetLocalHash() == RemoteHash);
	}
};
#endif