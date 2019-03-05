// LoLaReceiver.h

#ifndef _LOLARECEIVER_h
#define _LOLARECEIVER_h

#include <Arduino.h>
#include <Transceivers\LoLaBuffer.h>

class LoLaReceiver : public LoLaBuffer
{
private:
	TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE> ReceiverPacket;

	uint8_t IncomingContentSize = 0;


public:
	PacketDefinition * GetIncomingDefinition()
	{
		return BufferPacket->GetDefinition();
	}

	ILoLaPacket* GetIncomingPacket()
	{
		return BufferPacket;
	}

	bool Setup(LoLaPacketMap* packetMap, LoLaCryptoEncoder* cryptoEncoder)
	{
		if (LoLaBuffer::Setup(packetMap, cryptoEncoder))
		{
			BufferPacket = &ReceiverPacket;
			for (uint8_t i = 0; i < GetBufferSize(); i++)
			{
				GetBuffer()[i] = 0;
			}
			BufferPacket->SetDefinition(nullptr);

			return true;
		}
		return false;
	}

	//uint8_t IncomingToken = 0;
	bool ReceivePacket()
	{
		//Get token as early as possible.
		//IncomingToken = GetCryptoToken(0);//No latency compensation on receiver.

		if (BufferSize > 0 && BufferSize < LOLA_PACKET_MAX_PACKET_SIZE)
		{
			IncomingContentSize = PacketDefinition::GetContentSize(BufferSize);
			//Hash everything but the CRC at the start.
			CalculatorCRC.Reset();
			CalculatorCRC.Update(BufferPacket->GetRawContent(), IncomingContentSize);

			if (CalculatorCRC.GetCurrent() != BufferPacket->GetMACCRC())
			{
				//CRC Rejected.
				return false;
			}

			if (CryptoEnabled &&
				!Encoder->Decode(BufferPacket->GetRawContent(), IncomingContentSize, CryptoBuffer))
			{
				return false;
			}

			//Find a packet definition from map.
			if (BufferPacket->SetDefinition(PacketMap->GetDefinition(BufferPacket->GetDataHeader())) &&
				IncomingContentSize == BufferPacket->GetDefinition()->GetContentSize())
			{
				return true;
			}
		}

		BufferSize = 0;
		BufferPacket->SetDefinition(nullptr);

		return false;
	}
};

#endif