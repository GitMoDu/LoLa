// LoLaBuffer.h

#ifndef _LOLABUFFER_h
#define _LOLABUFFER_h

#include <Arduino.h>
#include <Crypto\TinyCRC.h>
#include <Crypto\ISeedSource.h>

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
	uint8_t CRCIndex = 0;
	///

	///Crypto validation.
	ISeedSource* CryptoSeed = nullptr;
	///

	uint8_t BufferSize = 0;
	ILoLaPacket* BufferPacket = nullptr;

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
	virtual bool Setup(LoLaPacketMap* packetMap)
	{
		PacketMap = packetMap;

		return PacketMap != nullptr && CryptoSeed != nullptr;
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

	void SetCryptoSeedSource(ISeedSource* cryptoSeedSource)
	{
		CryptoSeed = cryptoSeedSource;
	}

	inline uint8_t GetCryptoToken(const int8_t offsetMillis)
	{
		return CryptoSeed->GetToken(offsetMillis);
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