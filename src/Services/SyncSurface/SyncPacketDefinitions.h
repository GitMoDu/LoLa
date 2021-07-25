// SyncPacketDefinitions.h

#ifndef _SYNCPACKETDEFINITIONS_h
#define _SYNCPACKETDEFINITIONS_h

#include <Packet\PacketDefinition.h>
#include <Services\SyncSurface\ITrackedSurface.h>

#define PACKET_DEFINITION_SYNC_META_HEADER_OFFSET		0
#define PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET		1

#define SYNC_SERVICE_PACKET_DEFINITION_COUNT			(PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET+1)
#define SYNC_SERVICE_PACKET_MAX_PAYLOAD_SIZE			9 // From SyncDataPacketDefinition

template <const uint8_t BaseHeader>
class SyncDataPacketDefinition : public PacketDefinition
{
public:
	SyncDataPacketDefinition(ILoLaService* service) :
		PacketDefinition(service,
			BaseHeader + PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET,
			1 + 8) // 1 Byte for block index and 8 bytes for block data.
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncData for "));
		Service->PrintName(serial);
	}
#endif
};


template <const uint8_t BaseHeader>
class SyncMetaPacketDefinition : public PacketDefinition
{
public:
	SyncMetaPacketDefinition(ILoLaService* service) :
		PacketDefinition(service,
			BaseHeader + PACKET_DEFINITION_SYNC_META_HEADER_OFFSET,
			1 + 1) // 1 Byte id
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncMeta for "));
		Service->PrintName(serial);
	}
#endif
};
#endif