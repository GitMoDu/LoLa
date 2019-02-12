// LoLaSender.h

#ifndef _LOLASENDER_h
#define _LOLASENDER_h

#include <Arduino.h>
#include <Transceivers\LoLaBuffer.h>

class LoLaSender : public LoLaBuffer
{
private:
	PacketDefinition* AckDefinition = nullptr;

	TemplateLoLaPacket<LOLA_PACKET_MIN_WITH_ID_SIZE> AckPacket;

	///For use of estimated latency features
#ifdef USE_LATENCY_COMPENSATION
	uint8_t ETTM = 0;//Estimated transmission time in millis.
#endif
	///

	//Optimization helper.
	PacketDefinition* OutgoingDefinition = nullptr;

public:
	//Fast Ack, Nack, Ack with Id and Nack with Id packet sender, writes directly to output.
	bool SendPacket(ILoLaPacket* transmitPacket)
	{
		if (transmitPacket != nullptr)
		{
			OutgoingDefinition = transmitPacket->GetDefinition();
			CalculatorCRC.Reset();

			//Crypto starts at the start of the hash.
#ifdef USE_LATENCY_COMPENSATION
			CalculatorCRC.Update(GetCryptoSeed(ETTM));
#else
			CalculatorCRC.Update(GetCryptoSeed(0));
#endif
			//

			CalculatorCRC.Update(OutgoingDefinition->GetHeader());

			BufferIndex = LOLA_PACKET_SUB_HEADER_START;
			if (transmitPacket->GetDefinition()->HasId())
			{
				BufferIndex++;
				CalculatorCRC.Update(transmitPacket->GetId());
			}

			for (uint8_t i = 0; i < OutgoingDefinition->GetPayloadSize(); i++)
			{
				CalculatorCRC.Update(transmitPacket->GetPayload()[i]);
			}
			transmitPacket->GetRaw()[LOLA_PACKET_MACCRC_INDEX] = CalculatorCRC.GetCurrent();
			BufferSize = OutgoingDefinition->GetTotalSize();
			BufferPacket = transmitPacket;
			PacketsProcessed++;

			return true;
		}

		return false;
	}

	bool SendAck(PacketDefinition* payloadDefinition, const uint8_t id)
	{
		CalculatorCRC.Reset();

		//Crypto starts at the start of the hash.
#ifdef USE_LATENCY_COMPENSATION
		CalculatorCRC.Update(GetCryptoSeed(ETTM));
#else
		CalculatorCRC.Update(GetCryptoSeed(0));
#endif		
		//

		BufferPacket = &AckPacket;
		BufferPacket->SetDefinition(AckDefinition);
		CalculatorCRC.Update(PACKET_DEFINITION_ACK_HEADER);

		//Payload, header and ID.
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
		}

		return false;
	}

#ifdef USE_LATENCY_COMPENSATION
	void SetETTM(const int8_t ettm)
	{
		ETTM = ettm/2;
	}
#endif
};

#endif