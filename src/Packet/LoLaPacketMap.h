// LoLaPacketMapper.h

#ifndef _LOLAPACKETMAPPER_h
#define _LOLAPACKETMAPPER_h

#include <Packet\PacketDefinition.h>

#define LOLA_PACKET_MAP_TOTAL_SIZE 20 //255 //Reduce this to the highest header value in the mapping, to reduce memory usage.
#define PACKET_DEFINITION_ACK_HEADER 0x01

#define PACKET_DEFINITION_LINK_HEADER 0x02
#define PACKET_DEFINITION_LINK_ACK_HEADER 0x03

#define PACKET_DEFINITION_XACK_PAYLOAD_SIZE 2 //Payload Header and optional Id.

class AckPacketDefinition : public PacketDefinition
{
public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC; }
	uint8_t GetHeader() { return PACKET_DEFINITION_ACK_HEADER; }
	uint8_t GetPayloadSize() { return PACKET_DEFINITION_XACK_PAYLOAD_SIZE; }
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

		//Add base mappings
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

		if (Mapping[header] != nullptr)
		{
			return Mapping[header];
		}
		else
		{
			return nullptr;
		}		
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
				serial->print(F("\tMapping  "));
				serial->print(i, HEX);
				serial->print(F(": "));
				Mapping[i]->Debug(serial);
				serial->println();
			}
		}

		//Benchmark packet definition get mapping
		uint32_t Start, End;
		uint8_t Header = PACKET_DEFINITION_ACK_HEADER;
		uint8_t BenchmarkCounter = 0;
		PacketDefinition* BenchmarkDefinition = nullptr;

		Start = micros();
		BenchmarkDefinition = GetDefinition(Header);
		if (BenchmarkDefinition != nullptr)
		{
			BenchmarkCounter++;
			End = micros();
		}
		else
		{
			End = micros();
		}

		if (BenchmarkCounter > 0)
		{
			serial->print(F("Packet header mapping benchmark took: "));
			serial->print(End - Start);
			serial->print(F(" us."));
			serial->println();
		}
		else
		{
			serial->println(F("Packet header mapping benchmark failed."));
		}
	}
#endif
};
#endif

