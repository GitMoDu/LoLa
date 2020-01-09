// LoLaPacketMapper.h

#ifndef _LOLAPACKETMAPPER_h
#define _LOLAPACKETMAPPER_h

#include <Packet\PacketDefinition.h>
#include <LoLaDefinitions.h>
#include <IPacketListener.h>


class ILoLaPacketMap
{
protected:
	uint8_t MaxMappingIndex = 0;

#ifdef DEBUG_LOLA
	uint8_t UsedMappings = 0;
#endif

public:
#ifdef DEBUG_LOLA
	uint8_t GetCount()
	{
		return UsedMappings;
	}
#endif

	const uint8_t GetMaxIndex()
	{
		return MaxMappingIndex;
	}

public:
	virtual bool RegisterMapping(PacketDefinition* packetDefinition)
	{
		return false;
	}

	virtual IPacketListener* GetServiceAt(const uint8_t index)
	{
		return nullptr;
	}
};

class LoLaPacketMap : public ILoLaPacketMap
{
public:
	class AckPacketDefinition : public PacketDefinition
	{
	public:
		AckPacketDefinition() :
			PacketDefinition(nullptr,
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
private:
	AckPacketDefinition DefinitionACK;

protected:
	PacketDefinition* Mapping[MAX_MAPPING_SIZE];

public:
	LoLaPacketMap() : DefinitionACK()
	{
		ResetMapping();
	}

	void ResetMapping()
	{
		for (uint8_t i = 0; i < MAX_MAPPING_SIZE; i++)
		{
			Mapping[i] = nullptr;
		}
		MaxMappingIndex = 0;

#ifdef DEBUG_LOLA
		UsedMappings = 0;
#endif

		//Add base mappings.
		RegisterMapping(&DefinitionACK);
	}

	virtual bool RegisterMapping(PacketDefinition* packetDefinition)
	{
		if (packetDefinition->Header < MAX_MAPPING_SIZE &&
			packetDefinition != nullptr &&
			Mapping[packetDefinition->Header] == nullptr &&
			packetDefinition->Validate())
		{
			Mapping[packetDefinition->Header] = packetDefinition;

			if (packetDefinition->Header > MaxMappingIndex)
			{
				MaxMappingIndex = packetDefinition->Header;
			}

#ifdef DEBUG_LOLA
			UsedMappings++;
#endif
			return true;
		}

		return false;
	}

	IPacketListener* GetServiceAt(const uint8_t index)
	{
		if (index <= MaxMappingIndex &&
			Mapping[index] != nullptr)
		{
			return Mapping[index]->Service;
		}

		return nullptr;
	}

	PacketDefinition* GetDefinition(const uint8_t header)
	{
		if (header <= MaxMappingIndex)
		{
			return Mapping[header];
		}
		else
		{
#ifdef DEBUG_LOLA
			Serial.println(F("GetDefinition index out of bounds."));
#endif
			return nullptr;
		}
	}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print(F("Packet map memory space: "));
		serial->print(MAX_MAPPING_SIZE * sizeof(PacketDefinition*));
		serial->println(F(" bytes."));

		serial->print(F("MaxMappingIndex: "));
		serial->println(MaxMappingIndex);

		serial->print(F("Packet map usage: "));
		serial->print((GetCount() * 100) / MAX_MAPPING_SIZE);
		serial->println(F("% ."));

		serial->print(F("Packet mappings: "));
		serial->println(GetCount());

		uint8_t MaxServiceHeader = 0;

		for (uint8_t i = 0; i < MAX_MAPPING_SIZE; i++)
		{
			if (Mapping[i] != nullptr)
			{
				if (Mapping[i]->Header > MaxServiceHeader)
				{
					MaxServiceHeader = Mapping[i]->Header;
				}

				serial->print(' ');
				serial->print(i);
				serial->print(F(": "));
				Mapping[i]->Debug(serial);
				serial->println();
			}
		}

		serial->println(F("Services:"));
		IPacketListener* service = nullptr;

		uint8_t ServiceCount = 0;
		for (uint8_t i = 1; i < MAX_MAPPING_SIZE; i++)
		{
			if (Mapping[i] != nullptr)
			{
				if (Mapping[i]->Service != service)
				{
					ServiceCount++;
					service = Mapping[i]->Service;
					serial->print(' ');

					service->PrintName(serial);
					serial->println();
				}
			}
		}

		serial->print(F("Max Header: "));
		serial->println(MaxServiceHeader);

		serial->print(F("Total Mapped Definitions: "));
		serial->println(GetCount());

		serial->print(F("Total Mapped Services: "));
		serial->println(ServiceCount);
	}
#endif
};
#endif