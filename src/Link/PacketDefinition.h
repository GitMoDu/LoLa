// PacketDefinition.h

#ifndef _PACKETDEFINITION_h
#define _PACKETDEFINITION_h

#include <stdint.h>
#include <LoLaDefinitions.h>


//	Packet:
//	________
//	MAC0		MAC/CRC	4 Bytes.
//	MAC1
//	MAC2
//	MAC3
//	________
//	ID			Id rolling counter 1 Byte.
//	________
//  HEADER		Indexed header 1 Byte.
//	________
//	PAYLOAD0	Payload N Bytes.
//	PAYLOAD1
//	PAYLOADN
//	________

class PacketDefinition
{
public:
	static const uint8_t LOLA_PACKET_MACCRC_INDEX = 0;
	static const uint8_t LOLA_PACKET_MACCRC_SIZE = 4; // sizeof(uint32_t)
	static const uint8_t LOLA_PACKET_ID_INDEX = (LOLA_PACKET_MACCRC_INDEX + LOLA_PACKET_MACCRC_SIZE);
	static const uint8_t LOLA_PACKET_HEADER_INDEX = LOLA_PACKET_ID_INDEX + 1;
	static const uint8_t LOLA_PACKET_PAYLOAD_INDEX = LOLA_PACKET_HEADER_INDEX + 1;

	static const uint8_t LOLA_PACKET_BASE_CONTENT_SIZE = 2;	// Id + Header.
	static const uint8_t LOLA_PACKET_MIN_PACKET_TOTAL_SIZE = LOLA_PACKET_MACCRC_SIZE + LOLA_PACKET_BASE_CONTENT_SIZE;	// CRC + Id + Header.
	/*static const uint8_t LOLA_PACKET_MAX_PACKET_TOTAL_SIZE = 64;
	static const uint8_t LOLA_PACKET_MAX_CONTENT_SIZE = LOLA_PACKET_MAX_PACKET_TOTAL_SIZE - LOLA_PACKET_BASE_CONTENT_SIZE;
	static const uint8_t LOLA_PACKET_MAX_PAYLOAD_SIZE = LOLA_PACKET_MAX_PACKET_TOTAL_SIZE - LOLA_PACKET_MIN_PACKET_TOTAL_SIZE;*/


	//Reserved [0] for Ack.
	static const uint8_t PACKET_DEFINITION_ACK_HEADER = 0xFF;

	// Reserved [1;9] for Link service.
	static const uint8_t PACKET_DEFINITION_LINK_HEADER_COUNT = 9;
	static const uint8_t PACKET_DEFINITION_LINK_START_HEADER = PACKET_DEFINITION_ACK_HEADER - PACKET_DEFINITION_LINK_HEADER_COUNT;
	static const uint8_t PACKET_DEFINITION_LINK_END_HEADER = PACKET_DEFINITION_ACK_HEADER - 1;
	static const uint8_t PACKET_DEFINITION_LINK_HEADER_I_CAN_HOST = PACKET_DEFINITION_LINK_START_HEADER;
	static const uint8_t PACKET_DEFINITION_LINK_HEADER_I_WANT_LINK = PACKET_DEFINITION_LINK_START_HEADER + 1;

	// User Services header start.
	static const uint8_t PACKET_DEFINITION_USER_HEADERS_START = 0;
	static const uint8_t PACKET_DEFINITION_USER_HEADERS_END = PACKET_DEFINITION_LINK_START_HEADER;


public:
	static uint8_t GetContentSize(const uint8_t totalSize)
	{
		return totalSize - LOLA_PACKET_ID_INDEX;
	}

	static uint8_t GetPayloadSizeFromContentSize(const uint8_t contentSize)
	{
		return contentSize - (LOLA_PACKET_PAYLOAD_INDEX - LOLA_PACKET_ID_INDEX);
	}

	static constexpr uint8_t GetTotalSize(const uint8_t payloadSize)
	{
		return LOLA_PACKET_PAYLOAD_INDEX + payloadSize;
	}
};
#endif