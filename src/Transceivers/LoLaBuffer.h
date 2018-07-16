// LoLaBuffer.h

#ifndef _LOLABUFFER_h
#define _LOLABUFFER_h

#include <Arduino.h>
#include <Crypto\LoLaCrypto.h>

#include <Packet\PacketDefinition.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#define LOLA_BUFFER_SIZE 

class LoLaBuffer : public LoLaCrypto
{
protected:
	///Packet Mapper reuses the same object.
	LoLaPacketMap * PacketMap;
	///

	///Link health.
	//uint64_t BytesProcessed = 0;
	uint32_t PacketsProcessed = 0;
	uint32_t PacketsFailed = 0;
	///

	///Buffer use to store input/output packet raw data.
	uint8_t BufferIndex = 0;
	///

protected:
	uint8_t BufferSize = 0;
	ILoLaPacket* BufferPacket = nullptr;

	PacketDefinition * FindPacketDefinition(const uint8_t header)
	{
		if (PacketMap != nullptr)
		{
			return PacketMap->GetDefinition(header);
		}
		return nullptr;
	}
	
public:
	uint8_t * GetBuffer()
	{
		return BufferPacket->GetRaw();
	}

	void SetBufferSize(const uint8_t size)
	{
		BufferSize = size;
	}

	uint8_t GetBufferSize()
	{
		return BufferSize;
	}

	virtual bool Setup(LoLaPacketMap* packetMap)
	{
		for (uint8_t i = 0; i < GetBufferSize(); i++)
		{
			GetBuffer()[i] = 0;
		}

		PacketMap = packetMap;

		return PacketMap != nullptr;
	}

#ifdef DEBUG_LOLA
	void Debug(Stream * serial)
	{
		serial->print(F("|"));
		for (uint8_t i = 0; i < BufferSize; i++)
		{
			serial->print(BufferPacket->GetRaw()[i]);
			serial->print(F("|"));
		}
		serial->println();
	}
#endif // DEBUG_LOLA

};

#endif