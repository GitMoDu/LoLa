// LoLaPacket.h

#ifndef _LOLAPACKET_h
#define _LOLAPACKET_h

#include <Arduino.h>
#include <Packet\PacketDefinition.h>

#define LOLA_PACKET_HEADER_INDEX		0
#define LOLA_PACKET_MACCRC_INDEX		1
#define LOLA_PACKET_SUB_HEADER_START	(LOLA_PACKET_MACCRC_INDEX+1)

#define LOLA_PACKET_MIN_SIZE			(LOLA_PACKET_SUB_HEADER_START + 1)
#define LOLA_PACKET_MIN_WITH_ID_SIZE	(LOLA_PACKET_MIN_SIZE + 1)

class ILoLaPacket
{
protected:
	PacketDefinition * Definition = nullptr;

public:
	virtual uint8_t * GetRaw() { return nullptr; }

public:
	PacketDefinition * GetDefinition()
	{
		return Definition;
	}

	void ClearDefinition()
	{
		Definition = nullptr;
	}

	uint8_t GetId()
	{
		if (Definition != nullptr && Definition->HasId())
		{
			return GetRaw()[LOLA_PACKET_SUB_HEADER_START];
		}
		else
		{
			return 0;
		}
	}

	bool SetId(const uint8_t id)
	{
		if (Definition != nullptr && Definition->HasId())
		{
			GetRaw()[LOLA_PACKET_SUB_HEADER_START] = id;
			return true;
		}
		else
		{
			return false;
		}
	}

	uint8_t GetMACCRC()
	{
		return GetRaw()[LOLA_PACKET_MACCRC_INDEX];
	}

	uint8_t GetDataHeader()
	{
		return GetRaw()[LOLA_PACKET_HEADER_INDEX];
	}

	void SetDefinition(PacketDefinition* definition)
	{
		if (definition != nullptr)
		{
			GetRaw()[LOLA_PACKET_HEADER_INDEX] = definition->GetHeader();
		}
		Definition = definition;
	}

	uint8_t * GetPayload()
	{
		uint8_t Offset = LOLA_PACKET_SUB_HEADER_START;
		if (Definition != nullptr && Definition->HasId())
		{
			Offset++;
		}

		return &GetRaw()[Offset];
	}
};

template <const uint8_t DataSize>
class TemplateLoLaPacket : public ILoLaPacket
{
private:
	uint8_t Data[DataSize];
public:
	uint8_t * GetRaw()
	{
		return Data;
	}
};
#endif