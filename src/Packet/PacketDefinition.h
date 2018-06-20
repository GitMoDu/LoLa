// PacketDefinition.h

#ifndef _PACKETDEFINITION_h
#define _PACKETDEFINITION_h

#include <Arduino.h>

///Configurations for packets.
#define PACKET_DEFINITION_MASK_CUSTOM_1		 B00000001
#define PACKET_DEFINITION_MASK_CUSTOM_2		 B00000010
#define PACKET_DEFINITION_MASK_CUSTOM_3		 B00000100
#define PACKET_DEFINITION_MASK_CUSTOM_4		 B00001000
#define PACKET_DEFINITION_MASK_CUSTOM_5		 B00010000
#define PACKET_DEFINITION_MASK_HAS_ADDRESS   B00010000 //TODO: Implement in PacketManager.
#define PACKET_DEFINITION_MASK_HAS_ID		 B00100000 
#define PACKET_DEFINITION_MASK_HAS_PRIORITY  B01000000
#define PACKET_DEFINITION_MASK_HAS_ACK		 B10000000
#define PACKET_DEFINITION_MASK_BASIC		 B00000000

#define PACKET_DEFINITION_MAX_PACKET_SIZE 32

class PacketDefinition
{
public:
	virtual uint8_t GetConfiguration() { return 0; }
	virtual uint8_t GetHeader() { return 0; }
	virtual uint8_t GetPayloadSize() { return 0; }

	uint8_t GetTotalSize()
	{
		uint8_t TotalSize = 2;//1 Byte for Header, 1 Byte for MAC/CRC.

		if (HasId())
		{
			TotalSize += 1;//1 Byte for Id.
		}

		//if (HasAddress())
		//{
		//	TotalSize += 1;//1 Byte for Id.
		//}

		TotalSize += GetPayloadSize();//N Bytes for payload.

		return TotalSize;
	}

	bool HasPriority()
	{
		return GetConfiguration() & PACKET_DEFINITION_MASK_HAS_PRIORITY;
	}

	bool HasACK()
	{
		return GetConfiguration() & PACKET_DEFINITION_MASK_HAS_ACK;
	}

	bool HasId()
	{
		return GetConfiguration() & PACKET_DEFINITION_MASK_HAS_ID;
	}

	//bool HasAddress()
	//{
	//	return GetConfiguration() & PACKET_DEFINITION_MASK_HAS_ADDRESS;
	//}

#ifdef DEBUG_LOLA
	void Debug(Stream* serial)
	{
		serial->print(F(" Size: "));
		serial->print(GetTotalSize());
		serial->print(F(" |"));

		if (HasACK())
		{
			serial->print(F("ACK|"));
		}

		//if (HasAddress())
		//{
		//	serial->print(F("Address|"));
		//}

		if (HasId())
		{
			serial->print(F("Id|"));
		}

		if (HasPriority())
		{
			serial->print(F("Priority"));
		}

		//if (HasTopple())
		//{
		//	serial->print(F("Topple"));
		//}
	}
#endif
};
#endif