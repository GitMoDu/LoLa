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
			BufferPacket = transmitPacket;
			OutgoingDefinition = BufferPacket->GetDefinition();
			CalculatorCRC.Reset();

			//Crypto starts at the start of the hash.
#ifdef USE_LATENCY_COMPENSATION
			CalculatorCRC.Update(GetCryptoToken(ETTM));
#else
			CalculatorCRC.Update(GetCryptoToken(0));
#endif
			//Hash everything but the CRC at the start.
			CalculatorCRC.Update(BufferPacket->GetRawContent(), OutgoingDefinition->GetTotalSize() - LOLA_PACKET_HEADER_INDEX);

			BufferPacket->SetMACCRC(CalculatorCRC.GetCurrent());

			if (OutgoingDefinition->HasCrypto())
			{
				//TODO: Encode content.
			}

			BufferSize = OutgoingDefinition->GetTotalSize();			

			return true;
		}

		return false;
	}

	bool SendAck(PacketDefinition* payloadDefinition, const uint8_t id)
	{
		CalculatorCRC.Reset();

		//Crypto starts at the start of the hash.
#ifdef USE_LATENCY_COMPENSATION
		CalculatorCRC.Update(GetCryptoToken(ETTM));
#else
		CalculatorCRC.Update(GetCryptoToken(0));
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

		BufferPacket->SetMACCRC(CalculatorCRC.GetCurrent());
		BufferSize = AckDefinition->GetTotalSize();

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
		ETTM = ettm / 2;
	}

	uint8_t GetETTM()
	{
		return ETTM;
	}
#endif
};

#endif