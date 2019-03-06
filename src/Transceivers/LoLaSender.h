// LoLaSender.h

#ifndef _LOLASENDER_h
#define _LOLASENDER_h

#include <Arduino.h>
#include <Transceivers\LoLaBuffer.h>

class LoLaSender : public LoLaBuffer
{
private:
	PacketDefinition* AckDefinition = nullptr;

	TemplateLoLaPacket<LOLA_PACKET_MIN_SIZE_WITH_ID> AckPacket;

	///For use of estimated latency features
	uint8_t ETTM = 0;//Estimated transmission time in millis.
	///

	//Optimization helper.
	uint8_t OutgoingContentSize = 0;

public:
	//Writes directly to output buffer.
	bool SendPacket(ILoLaPacket* transmitPacket)
	{
		BufferPacket = transmitPacket;
		OutgoingContentSize = BufferPacket->GetDefinition()->GetContentSize();
		CalculatorCRC.Reset();

		if (*CryptoEnabled)
		{
			//Encode packet content.
			Encoder->Encode(BufferPacket->GetRawContent(), OutgoingContentSize);
		}

		//Hash everything but the CRC at the start.
		CalculatorCRC.Update(BufferPacket->GetRawContent(), OutgoingContentSize);
		BufferPacket->SetMACCRC(CalculatorCRC.GetCurrent());

		BufferSize = BufferPacket->GetDefinition()->GetTotalSize();

		return true;
	}

	bool SendAck(PacketDefinition* payloadDefinition, const uint8_t id)
	{
		AckPacket.SetDefinition(AckDefinition);
		AckPacket.GetPayload()[0] = payloadDefinition->GetHeader();
		AckPacket.SetId(id);

		return SendPacket(&AckPacket);
	}

	bool Setup(LoLaPacketMap* packetMap, LoLaCryptoEncoder* cryptoEncoder, bool* cryptoEnabled)
	{
		if (LoLaBuffer::Setup(packetMap, cryptoEncoder, cryptoEnabled))
		{
			AckDefinition = FindPacketDefinition(PACKET_DEFINITION_ACK_HEADER);
			if (AckDefinition != nullptr)
			{
				return true;
			}
		}

		return false;
	}

#ifdef USE_LATENCY_COMPENSATION
	void SetETTM(const int8_t ettm)
	{
		ETTM = ettm / 2;
	}

	uint8_t GetETTM()
	{
		return ETTM;
	}
#endif
};

#endif