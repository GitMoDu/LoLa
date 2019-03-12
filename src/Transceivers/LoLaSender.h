// LoLaSender.h

#ifndef _LOLASENDER_h
#define _LOLASENDER_h

#include <Arduino.h>
#include <Transceivers\LoLaBuffer.h>

//TODO: refactor into packet driver.
class LoLaSender : public LoLaBuffer
{
private:
	PacketDefinition* AckDefinition = nullptr;

	TemplateLoLaPacket<LOLA_PACKET_MIN_SIZE> AckPacket;

public:
	//Writes directly to output buffer.
	bool SendPacket(ILoLaPacket* transmitPacket)
	{
		BufferPacket.SetMACCRC(Encoder->Encode(transmitPacket->GetRawContent(), transmitPacket->GetDefinition()->GetContentSize(), BufferPacket.GetRawContent()));
		BufferSize = transmitPacket->GetDefinition()->GetTotalSize();

		return BufferSize > 0;
	}

	bool SendAck(const uint8_t header, const uint8_t id)
	{
		AckPacket.SetDefinition(AckDefinition);
		AckPacket.GetPayload()[0] = header;
		AckPacket.SetId(id);

		return SendPacket(&AckPacket);
	}

	bool Setup(LoLaPacketMap* packetMap, LoLaCryptoEncoder* cryptoEncoder)
	{
		if (LoLaBuffer::Setup(packetMap, cryptoEncoder))
		{
			AckDefinition = PacketMap->GetDefinition(PACKET_DEFINITION_ACK_HEADER);
			if (AckDefinition != nullptr)
			{
				return true;
			}
		}

		return false;
	}
};

#endif