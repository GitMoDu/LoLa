// PacketDefinition.h

#ifndef _PACKETDEFINITION_h
#define _PACKETDEFINITION_h

#include <stdint.h>

///Configurations for packets.
#define PACKET_DEFINITION_MASK_CUSTOM_5			B00000001
#define PACKET_DEFINITION_MASK_CUSTOM_4			B00000010
#define PACKET_DEFINITION_MASK_CUSTOM_3			B00000100
#define PACKET_DEFINITION_MASK_CUSTOM_2			B00001000
#define PACKET_DEFINITION_MASK_IS_ACK			B00010000
#define PACKET_DEFINITION_MASK_HAS_CRYPTO		B00100000	//TODO: Implement in Transceivers.
#define PACKET_DEFINITION_MASK_HAS_ID			B01000000 
#define PACKET_DEFINITION_MASK_HAS_ACK			B10000000
#define PACKET_DEFINITION_MASK_BASE				PACKET_DEFINITION_MASK_HAS_CRYPTO //Default is crypto on.
#define PACKET_DEFINITION_MASK_BASE_NO_CRYPTO	B00000000

#define LOLA_PACKET_MACCRC_INDEX				(0)
#define LOLA_PACKET_HEADER_INDEX				(LOLA_PACKET_MACCRC_INDEX + 1)
#define LOLA_PACKET_PAYLOAD_INDEX				(LOLA_PACKET_HEADER_INDEX + 1)

#define LOLA_PACKET_MIN_SIZE					(LOLA_PACKET_PAYLOAD_INDEX)	//CRC + Header + Payload
#define LOLA_PACKET_MIN_SIZE_WITH_ID			(LOLA_PACKET_MIN_SIZE + 1)		//CRC + Header + Payload + Id

#define LOLA_PACKET_MAX_PACKET_SIZE				32

class PacketDefinition
{
public:
	virtual uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASE; }
	virtual uint8_t GetHeader() { return 0; }
	virtual uint8_t GetPayloadSize() { return 0; }

#ifdef DEBUG_LOLA
	virtual void PrintName(Stream* serial) {}
#endif

	uint8_t GetContentSize()
	{
		return GetTotalSize() - LOLA_PACKET_HEADER_INDEX;
	}

	uint8_t GetTotalSize()
	{
		if (HasId())
		{
			return LOLA_PACKET_MIN_SIZE_WITH_ID + GetPayloadSize(); 
		}
		else
		{
			return LOLA_PACKET_MIN_SIZE + GetPayloadSize(); 
		}
	}

	bool IsAck()
	{
		return GetConfiguration() & PACKET_DEFINITION_MASK_IS_ACK;
	}	
	
	bool HasACK()
	{
		return GetConfiguration() & PACKET_DEFINITION_MASK_HAS_ACK;
	}

	bool HasId()
	{
		return GetConfiguration() & PACKET_DEFINITION_MASK_HAS_ID;
	}

	bool HasCrypto()
	{
		return GetConfiguration() & PACKET_DEFINITION_MASK_HAS_CRYPTO;
	}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		PrintName(serial);
		serial->print(F("\tSize: "));
		serial->print(GetTotalSize());
		serial->print(F(" \t|"));

		if (IsAck())
		{
			serial->print(F("IsACK|"));
		}else if (HasACK())
		{
			serial->print(F("ACK|"));
		}

		if (HasId())
		{
			serial->print(F("Id|"));
		}

		if (HasCrypto())
		{
			serial->print(F("Crypto"));
		}
	}
#endif
};
#endif