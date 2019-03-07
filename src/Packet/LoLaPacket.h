// LoLaPacket.h

#ifndef _LOLAPACKET_h
#define _LOLAPACKET_h

#include <Arduino.h>
#include <Packet\PacketDefinition.h>

class ILoLaPacket
{
private:
	PacketDefinition * Definition = nullptr;

public:
	virtual uint8_t * GetRaw() { return nullptr; }
	virtual uint8_t GetMaxSize() { return 0; }

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
		return GetRaw()[LOLA_PACKET_ID_INDEX];
	}

	void SetId(const uint8_t id)
	{
		GetRaw()[LOLA_PACKET_ID_INDEX] = id;
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

	bool SetDefinition(PacketDefinition* definition)
	{
		if (definition != nullptr)
		{
			GetRaw()[LOLA_PACKET_HEADER_INDEX] = definition->GetHeader();
		}
		Definition = definition;

		return Definition != nullptr;
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