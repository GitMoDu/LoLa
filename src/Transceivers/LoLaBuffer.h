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
private:

	////Function to copy 'length' elements from 'src' to 'dst'
	//void Copy(uint8_t* source, uint8_t* destination, uint8_t length) {
	//	memcpy(destination, source, sizeof(source[0])*length);
	//}

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
	/*	for (uint8_t i = 0; i < PACKET_DEFINITION_MAX_PACKET_SIZE; i++)
		{
			Buffer[i] = 0;
		}*/

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

	/*uint8_t* GetBuffer()
	{
		return &Buffer[0];
	}

	bool CopyToBuffer(uint8_t* buffer, uint8_t length)
	{
		if (length < PACKET_DEFINITION_MAX_PACKET_SIZE)
		{
			memcpy(&Buffer, buffer, sizeof(uint8_t)*length);
			BufferSize = length;
			return true;
		}
		else
		{
			BufferSize = 0;
			return false;
		}
	}

	bool CopyFromBuffer(uint8_t* buffer)
	{
		if (BufferSize > 0)
		{
			memcpy(buffer, &Buffer, sizeof(uint8_t)*BufferSize);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool CopyFromBuffer(uint8_t* buffer, const uint8_t startIndex)
	{
		if (BufferSize > 0)
		{
			memcpy(buffer, &Buffer[startIndex], (sizeof(uint8_t)*(BufferSize - startIndex)));
			return true;
		}
		else
		{
			return false;
		}
	}

	bool CopyFromBuffer(uint8_t* buffer, const uint8_t startIndex, const uint8_t length)
	{
		if (length == 0)
		{
			return true;
		}
		if (BufferSize > 0 && (BufferSize - startIndex) >= length)
		{
			memcpy(buffer, &Buffer[startIndex], (sizeof(uint8_t)*length));
			return true;
		}
		else
		{
			return false;
		}
	}*/
};

#endif

