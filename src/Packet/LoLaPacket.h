// LoLaPacket.h

#ifndef _LOLAPACKET_h
#define _LOLAPACKET_h

#include <Arduino.h>
#include <Packet\PacketDefinition.h>

#define LOLA_PACKET_MACCRC_INDEX			(0)
#define LOLA_PACKET_HEADER_INDEX			(LOLA_PACKET_MACCRC_INDEX + 1)
#define LOLA_PACKET_PAYLOAD_INDEX			(LOLA_PACKET_HEADER_INDEX + 1)

#define LOLA_PACKET_MIN_SIZE			(LOLA_PACKET_HEADER_INDEX + 1)
#define LOLA_PACKET_MIN_SIZE_WITH_ID	(LOLA_PACKET_MIN_SIZE + 1)

class ILoLaPacket
{
private:
	PacketDefinition * Definition = nullptr;

protected:
	virtual uint8_t * GetRaw() { return nullptr; }

public:
	virtual uint8_t GetMaxSize() { return 0; }

private:
	inline uint8_t GetIdIndex()
	{
		return LOLA_PACKET_PAYLOAD_INDEX + Definition->GetPayloadSize();
	}

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
		return GetRaw()[GetIdIndex()];
	}

	void SetId(const uint8_t id)
	{
		GetRaw()[GetIdIndex()] = id;
	}

	uint8_t GetMACCRC()
	{
		return GetRaw()[LOLA_PACKET_MACCRC_INDEX];
	}

	void SetMACCRC(const uint8_t crcValue)
	{
		GetRaw()[LOLA_PACKET_MACCRC_INDEX] = crcValue;
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



	uint8_t* GetPayload()
	{
		return &GetRaw()[LOLA_PACKET_PAYLOAD_INDEX];
	}

	uint8_t* GetRawContent()
	{
		return &GetRaw()[LOLA_PACKET_HEADER_INDEX];
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

	uint8_t GetMaxSize()
	{
		return DataSize;
	}
};
#endif