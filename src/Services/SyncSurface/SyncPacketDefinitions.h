// SyncPacketDefinitions.h

#ifndef _SYNCPACKETDEFINITIONS_h
#define _SYNCPACKETDEFINITIONS_h

#include <Packet\PacketDefinition.h>
#include <Services\SyncSurface\ITrackedSurface.h>

#define PACKET_DEFINITION_SYNC_META_HEADER_OFFSET		0
#define PACKET_DEFINITION_SYNC_META_PAYLOAD_SIZE		1
#define PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET		PACKET_DEFINITION_SYNC_META_HEADER_OFFSET + 1
#define PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE		4

#define SYNC_SERVICE_PACKET_DEFINITION_COUNT			3 

class SyncAbstractPacketDefinition : public PacketDefinition
{
private:
	uint8_t BaseHeader = 0;

public:
	SyncAbstractPacketDefinition() {}

	SyncAbstractPacketDefinition(const uint8_t baseHeader)
	{
		BaseHeader = baseHeader;
	}

	uint8_t GetHeader() { return BaseHeader; }

	virtual void SetBaseHeader(const uint8_t baseHeader)
	{
		BaseHeader = baseHeader;
	}

#ifdef DEBUG_LOLA
	ITrackedSurface* Owner = nullptr;
	void SetOwner(ITrackedSurface* trackedSurface)
	{
		Owner = trackedSurface;
	}
#endif
};

class SyncDataPacketDefinition : public SyncAbstractPacketDefinition
{
public:
	SyncDataPacketDefinition() : SyncAbstractPacketDefinition() {}

public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC; }

	//TODO: Refactor as template.
	void SetBaseHeader(const uint8_t baseHeader)
	{
		SyncAbstractPacketDefinition::SetBaseHeader(baseHeader + PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET);
	}

	uint8_t GetPayloadSize() { return PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE; }

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

class SyncMetaPacketDefinition : public SyncAbstractPacketDefinition
{
public:
	SyncMetaPacketDefinition() :SyncAbstractPacketDefinition() {}

public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC;}

	void SetBaseHeader(const uint8_t baseHeader)
	{
		SyncAbstractPacketDefinition::SetBaseHeader(baseHeader + PACKET_DEFINITION_SYNC_META_HEADER_OFFSET);
	}

	uint8_t GetPayloadSize() { return PACKET_DEFINITION_SYNC_META_PAYLOAD_SIZE; }

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