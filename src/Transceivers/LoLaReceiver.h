// LoLaReceiver.h

#ifndef _LOLARECEIVER_h
#define _LOLARECEIVER_h

#include <Arduino.h>
#include <Transceivers\LoLaBuffer.h>

class LoLaReceiver : public LoLaBuffer
{
private:
	TemplateLoLaPacket<PACKET_DEFINITION_MAX_PACKET_SIZE> ReceiverPacket;

	//Helper.
	uint8_t PayloadSize;

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
		if (BufferSize > 0 && BufferSize < PACKET_DEFINITION_MAX_PACKET_SIZE && ValidateMACCRC())
		{
			return true;
		}
		else
		{
			BufferSize = 0;
			BufferPacket->SetDefinition(nullptr);
			return false;
		}
	}

	bool ValidateMACCRC()
	{
		BufferPacket->SetDefinition(PacketMap->GetDefinition(BufferPacket->GetDataHeader()));

		if (BufferPacket->GetDefinition() != nullptr)
		{
			CRCIndex = 0;
			CalculatorCRC.Reset();

			//Crypto starts at the start of the hash.
			CalculatorCRC.Update(GetCryptoToken(0));//No latency compensation on receiver.

			CalculatorCRC.Update(BufferPacket->GetDataHeader());

			if (BufferPacket->GetDefinition()->HasId())
			{
				CalculatorCRC.Update(BufferPacket->GetId());
			}

			PayloadSize = BufferPacket->GetDefinition()->GetPayloadSize();
			for (uint8_t i = 0; i < PayloadSize; i++)
			{
				CalculatorCRC.Update(BufferPacket->GetPayload()[i]);//Payload byte
			}

			if (CalculatorCRC.GetCurrent() == BufferPacket->GetMACCRC())
			{
				return true;
			}
		}
		return false;
	}
};

#endif