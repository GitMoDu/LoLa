// LoLaPacket.h

#ifndef _LOLAPACKET_h
#define _LOLAPACKET_h

#include <Arduino.h>
#include <Packet\PacketDefinition.h>

class ILoLaPacket
{
private:
	PacketDefinition * Definition = nullptr;

	union ArrayToUint16 {
		byte array[sizeof(uint16_t)];
		uint16_t uint;
	} ATUI;

public:
	virtual uint8_t * GetRaw() { return nullptr; }
	virtual uint8_t GetMaxSize() { return 0; }

public:
	inline PacketDefinition * GetDefinition()
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

	uint16_t GetMACCRC()
	{
		ATUI.array[0] = GetRaw()[LOLA_PACKET_MACCRC_INDEX];
		ATUI.array[1] = GetRaw()[LOLA_PACKET_MACCRC_INDEX+1];

		return ATUI.uint;
	}

	void SetMACCRC(const uint16_t crcValue)
	{
		ATUI.uint = crcValue;
		GetRaw()[LOLA_PACKET_MACCRC_INDEX] = ATUI.array[0];
		GetRaw()[LOLA_PACKET_MACCRC_INDEX + 1] = ATUI.array[1];
	}

	uint8_t GetDataHeader()
	{
		return GetRaw()[LOLA_PACKET_HEADER_INDEX];
	}

	bool SetDefinition(PacketDefinition* definition)
	{
		Definition = definition;
		if (Definition != nullptr)
		{
			return true;
		}

		return false;
	}

	uint8_t* GetContent()
	{
		return &GetRaw()[LOLA_PACKET_HEADER_INDEX];
	}

	uint8_t* GetPayload()
	{
		return &GetRaw()[LOLA_PACKET_PAYLOAD_INDEX];
	}
};

template <const uint8_t DataSize>
class TemplateLoLaPacket : public ILoLaPacket
{
private:
	uint8_t Data[DataSize];

public:
	uint8_t* GetRaw()
	{
		return Data;
	}

	uint8_t GetMaxSize()
	{
		return DataSize;
	}
};
#endif