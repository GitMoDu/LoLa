// LoLaSender.h

#ifndef _LOLASENDER_h
#define _LOLASENDER_h

#include <Arduino.h>
#include <Transceivers\LoLaBuffer.h>

class LoLaSender : public LoLaBuffer
{
private:
	PacketDefinition* AckDefinition = nullptr;

	TemplateLoLaPacket<LOLA_PACKET_MIN_SIZE> AckPacket;

	///For use of estimated latency features
	uint8_t ETTM = 0;//Estimated transmission time in millis.
	///

	//Optimization helper.
	PacketDefinition* OugoingDefinition = nullptr;

public:
	//Writes directly to output buffer.
	bool SendPacket(ILoLaPacket* transmitPacket)
	{
		OugoingDefinition = transmitPacket->GetDefinition();

		CalculatorCRC.Reset();

		//Encode packet content, if crypto is enabled.
		Encoder->Encode(transmitPacket->GetRawContent(), OugoingDefinition->GetContentSize(), BufferPacket.GetRawContent());

		//TODO: Use encryption tag (truncated) when crypto is on, instead of crc.


		//Hash everything but the CRC at the start.
		CalculatorCRC.Update(BufferPacket.GetRawContent(), OugoingDefinition->GetContentSize());
		BufferPacket.SetMACCRC(CalculatorCRC.GetCurrent());

		BufferSize = OugoingDefinition->GetTotalSize();

		return BufferSize > 0 ;
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