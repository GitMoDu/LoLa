// PacketDefinition.h

#ifndef _PACKETDEFINITION_h
#define _PACKETDEFINITION_h

#include <stdint.h>
#include <LoLaDefinitions.h>


///Configurations for packets.
#define PACKET_DEFINITION_MASK_CUSTOM_5			B00000001
#define PACKET_DEFINITION_MASK_CUSTOM_4			B00000010
#define PACKET_DEFINITION_MASK_CUSTOM_3			B00000100
#define PACKET_DEFINITION_MASK_CUSTOM_2			B00001000
#define PACKET_DEFINITION_MASK_CUSTOM_1			B00010000
#define PACKET_DEFINITION_MASK_IS_ACK			B01000000
#define PACKET_DEFINITION_MASK_HAS_ACK			B10000000
#define PACKET_DEFINITION_MASK_BASIC			B00000000

// Packet: [MACCRC1|MACCRC2|HEADER|ID|PAYLOAD]

#define LOLA_PACKET_MACCRC_INDEX				(0)
#define LOLA_PACKET_MACCRC_SIZE					(2)
#define LOLA_PACKET_HEADER_INDEX				(LOLA_PACKET_MACCRC_INDEX + LOLA_PACKET_MACCRC_SIZE)
#define LOLA_PACKET_ID_INDEX					(LOLA_PACKET_HEADER_INDEX + 1)
#define LOLA_PACKET_PAYLOAD_INDEX				(LOLA_PACKET_ID_INDEX + 1)

#define LOLA_PACKET_MIN_PACKET_SIZE				(LOLA_PACKET_PAYLOAD_INDEX)	//CRC + Header + Id.

#define LOLA_PACKET_MAX_PACKET_SIZE				22 + LOLA_PACKET_MIN_PACKET_SIZE

class PacketDefinition
{
public:
	virtual const uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC; }
	virtual const uint8_t GetHeader() { return 0; }
	virtual const uint8_t GetPayloadSize() { return 0; }

#ifdef DEBUG_LOLA
	virtual void PrintName(Stream* serial) {}
#endif

	const uint8_t GetContentSize()
	{
		return GetTotalSize() - LOLA_PACKET_HEADER_INDEX;
	}

	static uint8_t GetContentSize(const uint8_t unknownTotalSize)
	{
		return min(LOLA_PACKET_MAX_PACKET_SIZE, unknownTotalSize) - LOLA_PACKET_HEADER_INDEX;
	}

	//No size checks.
	static uint8_t GetContentSizeQuick(const uint8_t unknownTotalSize)
	{
		return unknownTotalSize - LOLA_PACKET_HEADER_INDEX;
	}

	const uint8_t GetTotalSize()
	{
		return LOLA_PACKET_MIN_PACKET_SIZE + GetPayloadSize();
	}

	const bool IsAck()
	{
		return GetConfiguration() & PACKET_DEFINITION_MASK_IS_ACK;
	}

	const bool HasACK()
	{
		return GetConfiguration() & PACKET_DEFINITION_MASK_HAS_ACK;
	}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		PrintName(serial);
		serial->print(F("\tSize: "));
		serial->print(GetTotalSize());
		serial->print(F(" \t|"));

		if (IsAck())
		{
			serial->print(F("IsACK|"));
		}
		else if (HasACK())
		{
			serial->print(F("ACK|"));
		}
	}
#endif
};
#endif