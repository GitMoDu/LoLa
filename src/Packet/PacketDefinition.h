// PacketDefinition.h

#ifndef _PACKETDEFINITION_h
#define _PACKETDEFINITION_h

#include <stdint.h>
#include <LoLaDefinitions.h>
#include <Packet\ILoLaService.h>



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
	static const uint8_t LOLA_PACKET_MAX_PACKET_TOTAL_SIZE = 64;
	static const uint8_t LOLA_PACKET_MAX_CONTENT_SIZE = LOLA_PACKET_MAX_PACKET_TOTAL_SIZE - LOLA_PACKET_BASE_CONTENT_SIZE;
	static const uint8_t LOLA_PACKET_MAX_PAYLOAD_SIZE = LOLA_PACKET_MAX_PACKET_TOTAL_SIZE - LOLA_PACKET_MIN_PACKET_TOTAL_SIZE;


	//Reserved [0] for Ack.
	static const uint8_t PACKET_DEFINITION_ACK_HEADER = 0x00;

	// Reserved [1;9] for Link service.
	static const uint8_t PACKET_DEFINITION_LINK_START_HEADER = PACKET_DEFINITION_ACK_HEADER + 1;
	static const uint8_t PACKET_DEFINITION_LINK_HEADER_COUNT = 9;

	// User Services header start.
	static const uint8_t PACKET_DEFINITION_USER_HEADERS_START = PACKET_DEFINITION_LINK_START_HEADER + PACKET_DEFINITION_LINK_HEADER_COUNT;

public:
	const bool HasAck;

	const uint8_t Header;
	const uint8_t ContentSize; // Everything but the MAC/CRC.
	const uint8_t PayloadSize;
	const uint8_t TotalSize;

	ILoLaService* Service = nullptr;

public:
	PacketDefinition(ILoLaService* service,
		const uint8_t header,
		const uint8_t payloadSize,
		const uint8_t hasAck = false)
		: Header(header)
		, PayloadSize(payloadSize)
		, HasAck(hasAck)
		, ContentSize(LOLA_PACKET_BASE_CONTENT_SIZE + payloadSize)
		, TotalSize(LOLA_PACKET_MIN_PACKET_TOTAL_SIZE + payloadSize)
		, Service(service)
	{
	}

public:
	static uint8_t GetContentSize(const uint8_t totalSize)
	{
		return totalSize - LOLA_PACKET_ID_INDEX;
	}

	static constexpr uint8_t GetTotalSize(const uint8_t payloadSize)
	{
		return LOLA_PACKET_PAYLOAD_INDEX + payloadSize;
	}

#ifdef DEBUG_LOLA
public:
	virtual void PrintName(Stream* serial)
	{
		serial->print(F("No Name"));
	}

	void Debug(Stream* serial)
	{
		PrintName(serial);
		serial->print(F("\tSize: "));
		serial->print(TotalSize);
		serial->print(F(" \t|"));

		if (HasAck)
		{
			serial->print(F("ACK|"));
		}
	}
#endif
};
#endif