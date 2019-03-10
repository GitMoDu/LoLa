// LoLaBuffer.h

#ifndef _LOLABUFFER_h
#define _LOLABUFFER_h

#include <Arduino.h>
#include <LoLaCrypto\TinyCRC.h>
#include <LoLaCrypto\ISeedSource.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>


#include <Packet\PacketDefinition.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#define LOLA_BUFFER_SIZE 

class LoLaBuffer
{
protected:
	///Packet Mapper reuses the same object.
	LoLaPacketMap * PacketMap = nullptr;
	///

	///Buffer use to store input/output packet raw data.
	uint8_t BufferIndex = 0;
	///

	///CRC validation.
	TinyCrcModbus8 CalculatorCRC;
	///

	///Cryptography
	LoLaCryptoEncoder* Encoder = nullptr;
	///

	volatile uint8_t BufferSize = 0;
	TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE> BufferPacket;

protected:
	PacketDefinition * FindPacketDefinition(const uint8_t header)
	{
		if (PacketMap != nullptr)
		{
			return PacketMap->GetDefinition(header);
		}
		return nullptr;
	}

public:
	virtual bool Setup(LoLaPacketMap* packetMap, LoLaCryptoEncoder* cryptoEncoder)
	{
		PacketMap = packetMap;
		Encoder = cryptoEncoder;

		return PacketMap != nullptr && Encoder != nullptr;
	}
	
public:
	uint8_t* GetBuffer()
	{
		return BufferPacket.GetRaw();
	}

	void SetBufferSize(const uint8_t size)
	{
		BufferSize = size;
	}

	uint8_t GetBufferSize()
	{
		return BufferSize;
	}

#ifdef DEBUG_LOLA
	void Debug(Stream * serial)
	{
		serial->print(F("|"));
		for (uint8_t i = 0; i < BufferSize; i++)
		{
			serial->print(BufferPacket.GetRaw()[i]);
			serial->print(F("|"));
		}
		serial->println();
	}
#endif // DEBUG_LOLA

};

#endif