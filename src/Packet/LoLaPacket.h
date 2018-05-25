// LoLaPacket.h

#ifndef _LOLAPACKET_h
#define _LOLAPACKET_h

#include <Arduino.h>
#include <Packet\PacketDefinition.h>

#define LOLA_PACKET_HEADER_INDEX 0
#define LOLA_PACKET_MACCRC_INDEX 1
#define LOLA_PACKET_SUB_HEADER_START (LOLA_PACKET_MACCRC_INDEX+1)

#define LOLA_PACKET_MIN_SIZE (LOLA_PACKET_SUB_HEADER_START+1)

#define LOLA_PACKET_NO_PAYLOAD_SIZE LOLA_PACKET_MIN_SIZE
#define LOLA_PACKET_SLIM_SIZE (LOLA_PACKET_MIN_SIZE+5)

class ILoLaPacket
{
protected:
	PacketDefinition * Definition = nullptr;

public:
	PacketDefinition * GetDefinition()
	{
		return Definition;
	}

	void ClearDefinition()
	{
		Definition = nullptr;
	}

public:
	virtual void SetDefinition(PacketDefinition* definition) { Definition = definition; }
	virtual uint8_t * GetRaw() { return nullptr; }
	virtual uint8_t GetDataHeader() { return 0; }
	virtual uint8_t * GetPayload() { return nullptr; }
	virtual uint8_t GetId() { return 0; }
	virtual uint8_t GetMACCRC() { return 0; }
	virtual bool SetId(const uint8_t id) { return false; }
};

class LoLaPacketNoPayload : public ILoLaPacket
{
private:
	uint8_t Data[LOLA_PACKET_NO_PAYLOAD_SIZE];
public:
	void SetDefinition(PacketDefinition* definition)
	{
		if (definition != nullptr)
		{
			Data[LOLA_PACKET_HEADER_INDEX] = definition->GetHeader();
		}
		ILoLaPacket::SetDefinition(definition);
	}

	uint8_t * GetRaw()
	{
		return &Data[LOLA_PACKET_HEADER_INDEX];
	}

	uint8_t GetDataHeader()
	{
		return Data[LOLA_PACKET_HEADER_INDEX];
	}

	uint8_t GetId()
	{
		if (Definition != nullptr && Definition->HasId())
		{
			return Data[LOLA_PACKET_SUB_HEADER_START];
		}
		else
		{
			return 0;
		}
	}

	uint8_t GetMACCRC()
	{
		return Data[LOLA_PACKET_MACCRC_INDEX];
	}

	bool SetId(const uint8_t id)
	{
		if (Definition != nullptr && Definition->HasId())
		{
			Data[LOLA_PACKET_SUB_HEADER_START] = id;
			return true;
		}
		else
		{
			return false;
		}
	}
};

class LoLaPacketSlim : public ILoLaPacket
{
private:
	uint8_t Data[LOLA_PACKET_SLIM_SIZE];

public:
	uint8_t * GetRaw()
	{
		return &Data[LOLA_PACKET_HEADER_INDEX];
	}

	uint8_t GetDataHeader()
	{
		return Data[LOLA_PACKET_HEADER_INDEX];
	}

	void SetDefinition(PacketDefinition* definition)
	{
		if (definition != nullptr)
		{
			Data[LOLA_PACKET_HEADER_INDEX] = definition->GetHeader();
		}
		ILoLaPacket::SetDefinition(definition);
	}

	uint8_t * GetPayload()
	{
		uint8_t Offset = LOLA_PACKET_SUB_HEADER_START;
		if (Definition->HasId())
		{
			Offset++;
		}

		return &Data[Offset];
	}

	uint8_t GetId()
	{
		if (Definition != nullptr && Definition->HasId())
		{
			return Data[LOLA_PACKET_SUB_HEADER_START];
		}
		else
		{
			return 0;
		}
	}

	uint8_t GetMACCRC()
	{
		return Data[LOLA_PACKET_MACCRC_INDEX];
	}

	bool SetId(const uint8_t id)
	{
		if (Definition != nullptr && Definition->HasId())
		{
			Data[LOLA_PACKET_SUB_HEADER_START] = id;
			return true;
		}
		else
		{
			return false;
		}
	}
};

class LoLaPacketFull : public ILoLaPacket
{
private:
	uint8_t Data[PACKET_DEFINITION_MAX_PACKET_SIZE];

public:
	uint8_t * GetRaw()
	{
		return &Data[LOLA_PACKET_HEADER_INDEX];
	}

	uint8_t GetDataHeader()
	{
		return Data[LOLA_PACKET_HEADER_INDEX];
	}

	void SetDefinition(PacketDefinition* definition)
	{
		if (definition != nullptr)
		{
			Data[LOLA_PACKET_HEADER_INDEX] = definition->GetHeader();
		}
		ILoLaPacket::SetDefinition(definition);
	}

	uint8_t * GetPayload()
	{
		uint8_t Offset = LOLA_PACKET_SUB_HEADER_START;
		if (Definition->HasId())
		{
			Offset++;
		}

		return &Data[Offset];
	}

	uint8_t GetId()
	{
		if (Definition != nullptr && Definition->HasId())
		{
			return Data[LOLA_PACKET_SUB_HEADER_START];
		}
		else
		{
			return 0;
		}
	}

	uint8_t GetMACCRC()
	{
		return Data[LOLA_PACKET_MACCRC_INDEX];
	}

	bool SetId(const uint8_t id)
	{
		if (Definition != nullptr && Definition->HasId())
		{
			Data[LOLA_PACKET_SUB_HEADER_START] = id;
			return true;
		}
		else
		{
			return false;
		}
	}
};

#endif

