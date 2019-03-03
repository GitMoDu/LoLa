// LoLaReceiver.h

#ifndef _LOLARECEIVER_h
#define _LOLARECEIVER_h

#include <Arduino.h>
#include <Transceivers\LoLaBuffer.h>

class LoLaReceiver : public LoLaBuffer
{
private:
	TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE> ReceiverPacket;

public:
	PacketDefinition * GetIncomingDefinition()
	{
		return BufferPacket->GetDefinition();
	}

	ILoLaPacket* GetIncomingPacket()
	{
		return BufferPacket;
	}

	bool Setup(LoLaPacketMap* packetMap)
	{
		BufferPacket = &ReceiverPacket;
		if (LoLaBuffer::Setup(packetMap))
		{
			BufferPacket->SetDefinition(nullptr);
			return true;
		}
		return false;
	}

	bool ReceivePacket()
	{
		if (BufferSize > 0 && BufferSize < LOLA_PACKET_MAX_PACKET_SIZE)
		{
			BufferPacket->SetDefinition(PacketMap->GetDefinition(BufferPacket->GetDataHeader()));

			if (BufferPacket->GetDefinition() != nullptr)
			{
				CRCIndex = 0;
				CalculatorCRC.Reset();

				//Crypto starts at the beginning.
				CalculatorCRC.Update(GetCryptoToken(0));//No latency compensation on receiver.

				if (BufferPacket->GetDefinition()->HasCrypto())
				{
					//TODO: Decode content.
				}

				//Hash everything but the CRC at the start.
				CalculatorCRC.Update(BufferPacket->GetRawContent(), BufferPacket->GetDefinition()->GetContentSize());

				if (CalculatorCRC.GetCurrent() == BufferPacket->GetMACCRC())
				{
					return true;
				}
			}
		}

		BufferSize = 0;
		BufferPacket->SetDefinition(nullptr);
		return false;
	}
};

#endif