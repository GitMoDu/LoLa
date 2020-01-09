// PacketDefinition.h

#ifndef _PACKETDEFINITION_h
#define _PACKETDEFINITION_h

#include <stdint.h>
#include <LoLaDefinitions.h>
#include <IPacketListener.h>




// Packet: [MACCRC1|MACCRC2|HEADER|ID|PAYLOAD]

#define LOLA_PACKET_MACCRC_INDEX				(0)
#define LOLA_PACKET_MACCRC_SIZE					(LOLA_LINK_CRYPTO_MAC_CRC_SIZE)
#define LOLA_PACKET_HEADER_INDEX				(LOLA_PACKET_MACCRC_INDEX + LOLA_PACKET_MACCRC_SIZE)
#define LOLA_PACKET_ID_INDEX					(LOLA_PACKET_HEADER_INDEX + 1)
#define LOLA_PACKET_PAYLOAD_INDEX				(LOLA_PACKET_ID_INDEX + 1)

#define LOLA_PACKET_MIN_PACKET_SIZE				(LOLA_PACKET_PAYLOAD_INDEX)	//CRC + Header + Id.

#define LOLA_PACKET_MAX_PACKET_SIZE				64


class PacketDefinitionHelper
{
public:
	//No size checks.
	static uint8_t GetContentSizeQuick(const uint8_t unknownTotalSize)
	{
		return unknownTotalSize - LOLA_PACKET_HEADER_INDEX;
	}
};

class PacketDefinition
{
public:
	///Configurations for packets.
	static const uint8_t PACKET_DEFINITION_MASK_CUSTOM_7 = B00000010;
	static const uint8_t PACKET_DEFINITION_MASK_CUSTOM_6 = B00000100;
	static const uint8_t PACKET_DEFINITION_MASK_CUSTOM_5 = B00001000;
	static const uint8_t PACKET_DEFINITION_MASK_CUSTOM_4 = B00010000;
	static const uint8_t PACKET_DEFINITION_MASK_CUSTOM_3 = B00100000;
	static const uint8_t PACKET_DEFINITION_MASK_CUSTOM_2 = B01000000;
	static const uint8_t PACKET_DEFINITION_MASK_HAS_ACK = B10000000;
	static const uint8_t PACKET_DEFINITION_MASK_BASIC = B00000000;

public:
	const uint8_t Configuration;
	const uint8_t Header;
	const uint8_t PayloadSize;

	IPacketListener* Service = nullptr;

public:
	PacketDefinition(IPacketListener* service, const uint8_t header, const uint8_t payloadSize, const uint8_t configuration = PACKET_DEFINITION_MASK_BASIC)
		: Header(header)
		, PayloadSize(payloadSize)
		, Configuration(configuration)
	{
		Service = service;
	}

	virtual bool Validate()
	{
		return Service != nullptr;
	}

	const uint8_t GetContentSize()
	{
		return LOLA_PACKET_PAYLOAD_INDEX - LOLA_PACKET_HEADER_INDEX + PayloadSize;
	}

	const uint8_t GetTotalSize()
	{
		return LOLA_PACKET_MIN_PACKET_SIZE + PayloadSize;
	}

	bool HasACK()
	{
		return Configuration > 0;
		//return Configuration & PACKET_DEFINITION_MASK_HAS_ACK;
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
		serial->print(GetTotalSize());
		serial->print(F(" \t|"));
			
		if (HasACK())
		{
			serial->print(F("ACK|"));
		}
	}
#endif
};
#endif