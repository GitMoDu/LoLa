// LoLaPacketMapper.h

#ifndef _LOLAPACKETMAPPER_h
#define _LOLAPACKETMAPPER_h

#include <Packet\PacketDefinition.h>
#include <LoLaDefinitions.h>



class AckPacketDefinition : public PacketDefinition
{

public:
	AckPacketDefinition(IPacketListener* service) :
		PacketDefinition(service,
			PACKET_DEFINITION_ACK_HEADER,
			1)//Payload is original Header. Id is optional.
	{
	}

	virtual bool Validate()
	{
		//Ack is the only packet with no associated service.
		return true;
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Ack"));
	}
#endif
};

class LoLaPacketMap
{
private:
	AckPacketDefinition DefinitionACK;

#define ENABLE_PACKET_MAP_OPTIMIZATION

	// This value can be reduced to greatly optimize memory use.
	// But to do that safely: Packet map must be contiguous and ENABLE_PACKET_MAP_OPTIMIZATION must be enabled.
	// With no constrains, the default value of UINT8_MAX should be used.
	const uint8_t MAX_MAPPING_SIZE = UINT8_MAX;

	// Reduce this to the highest header value in the mapping, to reduce memory usage.

protected:
	uint8_t MappingCount = 0;
	PacketDefinition *Mapping[UINT8_MAX];

	bool MapClosed = false;

public:
	LoLaPacketMap() : DefinitionACK(nullptr)
	{
		ResetMapping();
	}

	PacketDefinition* GetAck() { return &DefinitionACK; }

	bool AddMapping(PacketDefinition* packetDefinition)
	{
		if (MapClosed ||
#ifdef ENABLE_PACKET_MAP_OPTIMIZATION
			MappingCount >= MAX_MAPPING_SIZE ||
#endif
			Mapping[packetDefinition->Header] != nullptr)
		{
			return false;
		}
		else
		{
			Mapping[packetDefinition->Header] = packetDefinition;
			MappingCount++;
			return true;
		}
	}

	PacketDefinition::IPacketListener* GetServiceAt(const uint8_t index)
	{
		if (
#ifdef ENABLE_PACKET_MAP_OPTIMIZATION
			index < MappingCount &&
#endif
			Mapping[index] != nullptr
			)
		{
			return Mapping[index]->Service;
		}

		return nullptr;
	}

	uint8_t GetCount()
	{
		return MappingCount;
	}

	bool CloseMap()
	{
		// Cannot reclose map.
		if (MapClosed)
		{
			return false;
		}

		for (uint8_t i = 0; i < MAX_MAPPING_SIZE; i++)
		{
			if (Mapping[i] != nullptr)
			{
				if (!Mapping[i]->Validate())
				{
					return false;
				}
			}
		}

		// We expect at least the Ack definition to be present.
		MapClosed = MappingCount > 0;

		return MapClosed;
	}

	void ResetMapping()
	{
		for (uint8_t i = 0; i < MAX_MAPPING_SIZE; i++)
		{
			Mapping[i] = nullptr;
		}
		MappingCount = 0;
		MapClosed = false;

		//Add base mappings.
		AddMapping(&DefinitionACK);
	}

	PacketDefinition* GetDefinition(const uint8_t header)
	{
#ifdef ENABLE_PACKET_MAP_OPTIMIZATION
		if (header >= MAX_MAPPING_SIZE)
		{
#ifdef DEBUG_LOLA
			Serial.println(F("GetDefinition index out of bounds."));
#endif
			return nullptr;
		}
#endif

		return Mapping[header];
	}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print(F("Packet map memory space: "));
		serial->print(MAX_MAPPING_SIZE * sizeof(PacketDefinition*));
		serial->println(F(" bytes."));


		serial->print(F("Packet map usage: "));
		serial->print((GetCount() * 100) / MAX_MAPPING_SIZE);
		serial->println(F("% ."));

		serial->print(F("Packet map real memory usage: "));
		serial->print(sizeof(LoLaPacketMap));
		serial->println(F(" bytes."));

		serial->print(F("Packet mappings: "));
		serial->println(GetCount());

		for (uint8_t i = 0; i < MAX_MAPPING_SIZE; i++)
		{
			if (Mapping[i] != nullptr)
			{
				serial->print(' ');
				serial->print(i);
				serial->print(F(": "));
				Mapping[i]->Debug(serial);
				serial->println();
			}
#ifdef ENABLE_PACKET_MAP_OPTIMIZATION
			else if (i < MappingCount)
			{

				Serial.println(F("[WARNING] PacketMap mapping not contiguous."));
			}
#endif
		}

		uint8_t ServiceCount = 0;
		serial->println(F("Services:"));
		PacketDefinition::IPacketListener* service = nullptr;

		for (uint8_t i = 0; i < MAX_MAPPING_SIZE; i++)
		{
			if (Mapping[i] != nullptr)
			{
				if (Mapping[i]->Service != service)
				{
					service = Mapping[i]->Service;
					ServiceCount++;
					serial->print(' ');

					service->PrintName(serial);
					serial->println();
				}
			}
#ifdef ENABLE_PACKET_MAP_OPTIMIZATION
			else if (i < MappingCount)
			{
				Serial.println(F("[WARNING] PacketMap mapping not contiguous."));
			}
#endif
		}

		serial->print(F("Total Mapped Services: "));
		serial->println(ServiceCount);
	}
#endif
};
#endif