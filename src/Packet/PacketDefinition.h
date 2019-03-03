// PacketDefinition.h

#ifndef _PACKETDEFINITION_h
#define _PACKETDEFINITION_h

#include <Arduino.h>

///Configurations for packets.
#define PACKET_DEFINITION_MASK_CUSTOM_1			B00000001
#define PACKET_DEFINITION_MASK_CUSTOM_2			B00000010
#define PACKET_DEFINITION_MASK_CUSTOM_3			B00000100
#define PACKET_DEFINITION_MASK_CUSTOM_4			B00001000
#define PACKET_DEFINITION_MASK_CUSTOM_5			B00010000
#define PACKET_DEFINITION_MASK_HAS_CRYPTO		B00100000	//TODO: Implement in Transceivers.
#define PACKET_DEFINITION_MASK_HAS_ID			B01000000 
#define PACKET_DEFINITION_MASK_HAS_ACK			B10000000
#define PACKET_DEFINITION_MASK_BASE				PACKET_DEFINITION_MASK_HAS_CRYPTO //Default is crypto on.
#define PACKET_DEFINITION_MASK_BASE_NO_CRYPTO	B00000000

#define PACKET_DEFINITION_MAX_PACKET_SIZE 32

class PacketDefinition
{
public:
	virtual uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASE; }
	virtual uint8_t GetHeader() { return 0; }
	virtual uint8_t GetPayloadSize() { return 0; }

#ifdef DEBUG_LOLA
	virtual void PrintName(Stream* serial)
	{

	}
#endif

	uint8_t GetTotalSize()
	{
		uint8_t TotalSize = 2;//1 Byte for Header, 1 Byte for MAC/CRC.

		if (HasId())
		{
			TotalSize += 1;//1 Byte for Id.
		}

		TotalSize += GetPayloadSize();//N Bytes for payload.

		return TotalSize;
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
		serial->print(F(" |"));

		if (HasACK())
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