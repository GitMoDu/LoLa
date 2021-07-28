// LoLaPacketMapper.h

#ifndef _LOLAPACKETMAPPER_h
#define _LOLAPACKETMAPPER_h

#include <LoLaDefinitions.h>
#include <Packet\PacketDefinition.h>
#include <Packet\ILoLaService.h>

class LoLaPacketMap
{
public:
	struct WrappedDefinition
	{
		PacketDefinition* Definition = nullptr;
	};

private:
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
	class AckPacketDefinition : public PacketDefinition
	{
	public:
		AckPacketDefinition() :
			PacketDefinition(nullptr,
				PACKET_DEFINITION_ACK_HEADER,
				2)//Payload is original Header and Id.
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
	PacketDefinition* Mapping[UINT8_MAX];

public:
	LoLaPacketMap() : DefinitionACK()
	{
		ResetMapping();
	}

	void ResetMapping()
	{
		for (uint8_t i = 0; i < UINT8_MAX; i++)
		{
			Mapping[i] = nullptr;
		}
		MaxMappingIndex = 0;

#ifdef DEBUG_LOLA
		UsedMappings = 0;
#endif

		Mapping[DefinitionACK.Header] = &DefinitionACK;
		MaxMappingIndex = DefinitionACK.Header;
#ifdef DEBUG_LOLA
		UsedMappings++;
#endif
	}

	virtual bool RegisterMapping(PacketDefinition* packetDefinition)
	{
		if (packetDefinition != nullptr && 
			packetDefinition->Service != nullptr &&
			Mapping[packetDefinition->Header] == nullptr)
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

	ILoLaService* GetServiceAt(const uint8_t index)
	{
		if (index <= MaxMappingIndex &&
			Mapping[index] != nullptr)
		{
			return Mapping[index]->Service;
		}

		return nullptr;
	}

	ILoLaService* GetServiceNotAckAt(const uint8_t header)
	{
		if (header <= MaxMappingIndex &&
			header != DefinitionACK.Header &&
			Mapping[header] != nullptr)
		{
			return Mapping[header]->Service;
		}

		return nullptr;
	}

	PacketDefinition* GetDefinitionNotAck(const uint8_t header)
	{
		if (header != DefinitionACK.Header && header <= MaxMappingIndex)
		{
			return Mapping[header];
		}
		else
		{
#ifdef DEBUG_LOLA
			Serial.println(F("GetDefinition Not Ack index out of bounds."));
#endif
			return nullptr;
		}
	}

	PacketDefinition* GetAckDefinition()
	{
		return &DefinitionACK;
	}

	const bool TryGetDefinition(const uint8_t header, WrappedDefinition* outDefinition)
	{
		if (header <= MaxMappingIndex)
		{
			outDefinition->Definition = Mapping[header];

			return outDefinition->Definition != nullptr;
		}
		else
		{
#ifdef DEBUG_LOLA
			Serial.println(F("TryGetDefinition index out of bounds."));
#endif
			return false;
		}
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
		serial->print(UINT8_MAX * sizeof(PacketDefinition*));
		serial->println(F(" bytes."));

		serial->print(F("MaxMappingIndex: "));
		serial->println(MaxMappingIndex);

		serial->print(F("Packet map usage: "));
		serial->print((GetCount() * 100) / UINT8_MAX);
		serial->println(F("% ."));

		serial->print(F("Packet mappings: "));
		serial->println(GetCount());

		uint8_t MaxServiceHeader = 0;

		for (uint8_t i = 0; i < UINT8_MAX; i++)
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
		ILoLaService* service = nullptr;

		uint8_t ServiceCount = 0;
		for (uint8_t i = 1; i < UINT8_MAX; i++)
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