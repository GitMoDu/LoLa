// LoLaPacketMapper.h

#ifndef _LOLAPACKETMAPPER_h
#define _LOLAPACKETMAPPER_h

#include <Packet\PacketDefinition.h>

#define LOLA_PACKET_MAP_TOTAL_SIZE					20 //255 //Reduce this to the highest header value in the mapping, to reduce memory usage.

//Reserved [0;1] for AckS
#define PACKET_DEFINITION_ACK_HEADER				0x00
#define PACKET_DEFINITION_ACK_ENCRYPTED_HEADER		0x01

//Reserved [2;3] for Link service.
#define PACKET_DEFINITION_LINK_HEADER				(PACKET_DEFINITION_ACK_ENCRYPTED_HEADER + 1)
#define PACKET_DEFINITION_LINK_WITH_ACK_HEADER		(PACKET_DEFINITION_LINK_HEADER + 1)

//User service range start
#define PACKET_DEFINITION_USER_HEADERS_START		(PACKET_DEFINITION_LINK_WITH_ACK_HEADER + 1)

class AckPacketDefinition : public PacketDefinition
{
public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASE_NO_CRYPTO | PACKET_DEFINITION_MASK_IS_ACK; }
	uint8_t GetHeader() { return PACKET_DEFINITION_ACK_HEADER; }
	uint8_t GetPayloadSize() {
		return 2;//Payload is original Header. Id is optional.
	}
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Ack\t"));
	}
#endif
};

class AckEncryptedPacketDefinition : public PacketDefinition
{
public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASE | PACKET_DEFINITION_MASK_IS_ACK; }
	uint8_t GetHeader() { return PACKET_DEFINITION_ACK_ENCRYPTED_HEADER; }
	uint8_t GetPayloadSize() {
		return 2;//Payload is original Header. Id is optional.
	}
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("AckEnc"));
	}
#endif
};

class LoLaPacketMap
{
private:
	AckPacketDefinition DefinitionACK;
	AckEncryptedPacketDefinition DefinitionACKEncrypted;
protected:
	uint8_t MappingSize = 0;
	PacketDefinition* Mapping[LOLA_PACKET_MAP_TOTAL_SIZE];

public:
	PacketDefinition * GetAck() { return &DefinitionACK; }

	bool AddMapping(PacketDefinition* packetDefinition)
	{
		if (MappingSize >= LOLA_PACKET_MAP_TOTAL_SIZE || Mapping[packetDefinition->GetHeader()] != nullptr)
		{
			return false;
		}
		else
		{
			Mapping[packetDefinition->GetHeader()] = packetDefinition;
			MappingSize++;
			return true;
		}
	}

	void ClearMapping()
	{
		for (uint8_t i = 0; i < LOLA_PACKET_MAP_TOTAL_SIZE; i++)
		{
			Mapping[i] = nullptr;
		}
		MappingSize = 0;

		//Add base mappings
		AddMapping(&DefinitionACK);
		AddMapping(&DefinitionACKEncrypted);
	}

	LoLaPacketMap()
	{
		ClearMapping();
	}

	PacketDefinition* GetDefinition(const uint8_t header)
	{
		if (header >= LOLA_PACKET_MAP_TOTAL_SIZE)
		{
			return nullptr;
		}

		return Mapping[header];
	}

	uint8_t GetSize()
	{
		return MappingSize;
	}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print(F("Packet map memory space: "));
		serial->print(LOLA_PACKET_MAP_TOTAL_SIZE * sizeof(PacketDefinition*));
		serial->println(F(" bytes."));


		serial->print(F("Packet map memory actual usage: "));
		serial->print(GetSize() * sizeof(PacketDefinition*));
		serial->println(F(" bytes."));

		serial->print(F("Packet mappings: "));
		serial->println(GetSize());

		for (uint8_t i = 0; i < LOLA_PACKET_MAP_TOTAL_SIZE; i++)
		{
			if (Mapping[i] != nullptr)
			{
				serial->print(' ');
				serial->print(i, HEX);
				serial->print(F(": "));
				Mapping[i]->Debug(serial);
				serial->println();
			}
		}
	}
#endif
};
#endif