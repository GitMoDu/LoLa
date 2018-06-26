// SyncPacketDefinitions.h

#ifndef _SYNCPACKETDEFINITIONS_h
#define _SYNCPACKETDEFINITIONS_h

#include <Packet\PacketDefinition.h>

#define PACKET_DEFINITION_SYNC_STATUS_HEADER_OFFSET 0
#define PACKET_DEFINITION_SYNC_STATUS_PAYLOAD_SIZE 5
#define PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET 1
#define PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE 4
#define PACKET_DEFINITION_SYNC_PROTOCOL_HEADER_OFFSET 2
#define PACKET_DEFINITION_SYNC_PROTOCOL_PAYLOAD_SIZE 1

#define SYNC_SERVICE_PACKET_DEFINITION_COUNT			3 

class SyncAbstractPacketDefinition : public PacketDefinition
{
private:
	uint8_t BaseHeader;
public:
	SyncAbstractPacketDefinition() {}
	SyncAbstractPacketDefinition(uint8_t baseHeader)
	{
		BaseHeader = baseHeader;
	}
public:
	virtual uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC; }
	uint8_t GetHeader() { return BaseHeader; }
	virtual void SetBaseHeader(const uint8_t baseHeader)
	{
		BaseHeader = baseHeader;
	}
};

class SyncDataPacketDefinition : public SyncAbstractPacketDefinition
{
public:
	SyncDataPacketDefinition() {}

public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ID; }
	void SetBaseHeader(const uint8_t baseHeader)
	{
		SyncAbstractPacketDefinition::SetBaseHeader(baseHeader + PACKET_DEFINITION_SYNC_DATA_HEADER_OFFSET);
	}
	uint8_t GetPayloadSize() { return PACKET_DEFINITION_SYNC_DATA_PAYLOAD_SIZE; }
};

class SyncReportPacketDefinition : public SyncAbstractPacketDefinition
{
public:
	SyncReportPacketDefinition() {}
public:
	virtual uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID;}
	void SetBaseHeader(const uint8_t baseHeader)
	{
		SyncAbstractPacketDefinition::SetBaseHeader(baseHeader + PACKET_DEFINITION_SYNC_STATUS_HEADER_OFFSET);
	}
	uint8_t GetPayloadSize() { return PACKET_DEFINITION_SYNC_STATUS_PAYLOAD_SIZE; }
};

class SyncProtocolPacketDefinition : public SyncAbstractPacketDefinition
{
public:
	SyncProtocolPacketDefinition() {}
public:
	virtual uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID; }
	void SetBaseHeader(const uint8_t baseHeader)
	{
		SyncAbstractPacketDefinition::SetBaseHeader(baseHeader + PACKET_DEFINITION_SYNC_PROTOCOL_HEADER_OFFSET);
	}
	uint8_t GetPayloadSize() { return PACKET_DEFINITION_SYNC_PROTOCOL_PAYLOAD_SIZE; }
};

#endif

