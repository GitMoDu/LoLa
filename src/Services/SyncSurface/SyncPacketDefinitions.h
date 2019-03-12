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


class SyncAbstractPacketDefinition : public PacketDefinition
{
public:
#ifdef DEBUG_LOLA
	ITrackedSurface* Owner = nullptr;
	void SetOwner(ITrackedSurface* trackedSurface)
	{
		Owner = trackedSurface;
	}
#endif
};

template <const uint8_t BaseHeader>
class SyncDataPacketDefinition : public SyncAbstractPacketDefinition
{
public:
	const uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC; }
	const uint8_t GetHeader() { return BaseHeader + PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET; }
	const uint8_t GetPayloadSize() { return PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncData "));
		if (Owner != nullptr)
		{
			Owner->PrintName(serial);
		}
	}
#endif
};

template <const uint8_t BaseHeader>
class SyncMetaPacketDefinition : public SyncAbstractPacketDefinition
{
public:
	const uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC;}
	const uint8_t GetHeader() { return BaseHeader + PACKET_DEFINITION_SYNC_META_HEADER_OFFSET; }
	const uint8_t GetPayloadSize() { return PACKET_DEFINITION_SYNC_META_PAYLOAD_SIZE; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("SyncMeta "));
		if (Owner != nullptr)
		{
			Owner->PrintName(serial);
		}
	}
#endif
};
#endif