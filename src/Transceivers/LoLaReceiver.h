// LoLaReceiver.h

#ifndef _LOLARECEIVER_h
#define _LOLARECEIVER_h

#include <Transceivers\LoLaBuffer.h>

//TODO: refactor into packet driver.
class LoLaReceiver : public LoLaBuffer
{
private:
	uint8_t IncomingContentSize = 0;

public:
	PacketDefinition * GetIncomingDefinition()
	{
		return BufferPacket.GetDefinition();
	}

	ILoLaPacket* GetIncomingPacket()
	{
		return &BufferPacket;
	}

	bool Setup(LoLaPacketMap* packetMap, LoLaCryptoEncoder* cryptoEncoder)
	{
		if (LoLaBuffer::Setup(packetMap, cryptoEncoder))
		{
			for (uint8_t i = 0; i < GetBufferSize(); i++)
			{
				GetBuffer()[i] = 0;
			}
			BufferPacket.SetDefinition(nullptr);

			return true;
		}
		return false;
	}

	bool ReceivePacket()
	{
		BufferPacket.SetDefinition(nullptr);
		if (BufferSize > 0 && BufferSize <= LOLA_PACKET_MAX_PACKET_SIZE)
		{
			IncomingContentSize = PacketDefinition::GetContentSize(BufferSize);
			
			//Hash everything and compare with the CRC at the start.
			if (BufferPacket.GetMACCRC() == Encoder->Decode(BufferPacket.GetRawContent(), IncomingContentSize))
			{
				//Find a packet definition from map.
				if (!BufferPacket.SetDefinition(PacketMap->GetDefinition(BufferPacket.GetDataHeader())) ||
					IncomingContentSize != BufferPacket.GetDefinition()->GetContentSize())
				{
					BufferPacket.SetDefinition(nullptr);
					BufferSize = 0;
				}
			}			
		}

		return BufferPacket.GetDefinition() != nullptr;
	}
};

#endif