// SyncPacketDefinitions.h

#ifndef _SYNCPACKETDEFINITIONS_h
#define _SYNCPACKETDEFINITIONS_h

#include <Packet\PacketDefinition.h>
#include <Services\SyncSurface\ITrackedSurface.h>

#define PACKET_DEFINITION_SYNC_META_HEADER_OFFSET		0
#define PACKET_DEFINITION_SYNC_META_PAYLOAD_SIZE		1
#define PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET		1
#define PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE		4

#define SYNC_SERVICE_PACKET_DEFINITION_COUNT			2

template <const uint8_t BaseHeader>
class SyncDataPacketDefinition : public PacketDefinition
{
public:
	SyncDataPacketDefinition(IPacketListener* service) :
		PacketDefinition(service,
			BaseHeader + PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET,
			PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncData "));
		//TODO: Get name from service.
	}
#endif
};

template <const uint8_t BaseHeader>
class SyncMetaPacketDefinition : public PacketDefinition
{
public:
	SyncMetaPacketDefinition(IPacketListener* service) :
		PacketDefinition(service,
			BaseHeader + PACKET_DEFINITION_SYNC_META_HEADER_OFFSET,
			PACKET_DEFINITION_SYNC_META_PAYLOAD_SIZE)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncMeta "));
		//TODO: Get name from service.
	}
#endif
};
#endif