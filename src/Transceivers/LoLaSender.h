// LoLaSender.h

#ifndef _LOLASENDER_h
#define _LOLASENDER_h

#include <Arduino.h>
#include <Transceivers\LoLaBuffer.h>

class LoLaSender : public LoLaBuffer
{
private:
	PacketDefinition* AckDefinition;

	LoLaPacketSlim AckPacket;

public:
	//Fast Ack, Nack, Ack with Id and Nack with Id packet sender, writes directly to output.
	bool SendPacket(ILoLaPacket* transmitPacket)
	{
		if (transmitPacket != nullptr && transmitPacket->GetDefinition() != nullptr)
		{
			CalculatorCRC.Reset();

			if (IsCryptoEnabled())
			{
				return false;
			}
			else
			{
				BufferPacket = transmitPacket;
				CalculatorCRC.Update(transmitPacket->GetDefinition()->GetHeader());

				BufferIndex = LOLA_PACKET_SUB_HEADER_START;
				if (transmitPacket->GetDefinition()->HasId())
				{
					BufferIndex++;
					CalculatorCRC.Update(transmitPacket->GetId());
				}

				for (uint8_t i = 0; i < transmitPacket->GetDefinition()->GetPayloadSize(); i++)
				{
					CalculatorCRC.Update(transmitPacket->GetPayload()[i]);
				}
				transmitPacket->GetRaw()[LOLA_PACKET_MACCRC_INDEX] = CalculatorCRC.GetCurrent();
				BufferSize = transmitPacket->GetDefinition()->GetTotalSize();
			}
			PacketsProcessed++;

			return true;
		}

		return false;
	}

	bool SendAck(PacketDefinition* payloadDefinition, const uint8_t id)
	{
		CalculatorCRC.Reset();

		if (IsCryptoEnabled())
		{
			return false;
		}
		else
		{
			BufferPacket = &AckPacket;
			BufferPacket->SetDefinition(AckDefinition);
			CalculatorCRC.Update(PACKET_DEFINITION_ACK_HEADER);

			//Payload, header and ID
			BufferPacket->GetPayload()[0] = payloadDefinition->GetHeader();
			CalculatorCRC.Update(payloadDefinition->GetHeader());

			if (payloadDefinition->HasId())
			{
				BufferPacket->GetPayload()[1] = id;
				CalculatorCRC.Update(id);
			}
			else
			{
				BufferPacket->GetPayload()[1] = 0;
				CalculatorCRC.Update(0);
			}

			BufferPacket->GetRaw()[LOLA_PACKET_MACCRC_INDEX] = CalculatorCRC.GetCurrent();
			BufferSize = AckDefinition->GetTotalSize();

			PacketsProcessed++;
		}

		return true;
	}

	bool Setup(LoLaPacketMap* packetMap)
	{
		if (LoLaBuffer::Setup(packetMap))
		{
			AckDefinition = FindPacketDefinition(PACKET_DEFINITION_ACK_HEADER);
			if (AckDefinition != nullptr)
			{
				return true;
			}
			else
			{
				AckDefinition = nullptr;
			}
		}

		return false;
	}
};
#endif

