// LoLaPacketMapper.h

#ifndef _LOLAPACKETMAPPER_h
#define _LOLAPACKETMAPPER_h

#include <Packet\PacketDefinition.h>
#include <LoLaDefinitions.h>



class AckPacketDefinition : public PacketDefinition
{
public:
	const uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_IS_ACK; }
	const uint8_t GetHeader() { return PACKET_DEFINITION_ACK_HEADER; }
	const uint8_t GetPayloadSize() {
		return 1;//Payload is original Header. Id is optional.
	}
#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Ack\t"));
	}
#endif
};

class LoLaPacketMap
{
private:
	AckPacketDefinition DefinitionACK;
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

		//Add base mappings.
		AddMapping(&DefinitionACK);
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