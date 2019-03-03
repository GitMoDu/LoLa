// LoLaSender.h

#ifndef _LOLASENDER_h
#define _LOLASENDER_h

#include <Arduino.h>
#include <Transceivers\LoLaBuffer.h>

class LoLaSender : public LoLaBuffer
{
private:
	PacketDefinition* AckDefinition = nullptr;
	PacketDefinition* AckEncryptedDefinition = nullptr;

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
		BufferPacket = transmitPacket;

		if (BufferPacket != nullptr)
		{
			OutgoingDefinition = BufferPacket->GetDefinition();
			CalculatorCRC.Reset();

			//Encrypt, then MAC.
			if (OutgoingDefinition->HasCrypto())
			{
				//TODO: Encode content.
			}

			//Hash everything but the CRC at the start.
			CalculatorCRC.Update(BufferPacket->GetRawContent(), OutgoingDefinition->GetContentSize());

			//Crypto starts at the start of the hash.
#ifdef USE_LATENCY_COMPENSATION
			BufferPacket->SetMACCRC(CalculatorCRC.Update(GetCryptoToken(ETTM)));
#else
			BufferPacket->SetMACCRC(CalculatorCRC.Update(GetCryptoToken(0)));
#endif			

			BufferSize = OutgoingDefinition->GetTotalSize();

			return true;
		}

		return false;
	}

	bool SendAck(PacketDefinition* payloadDefinition, const uint8_t id)
	{
		CalculatorCRC.Reset();
		BufferPacket = &AckPacket;

		if (payloadDefinition->HasCrypto())
		{
			OutgoingDefinition = AckEncryptedDefinition;
		}
		else
		{
			OutgoingDefinition = AckDefinition;
		}

		BufferPacket->SetDefinition(OutgoingDefinition);
		BufferPacket->GetPayload()[0] = payloadDefinition->GetHeader();
		BufferPacket->GetPayload()[1] = id;

		//Encrypt, then MAC.
		if (OutgoingDefinition->HasCrypto())
		{
			//TODO: Encrypt content.
		}

		//Hash everything but the CRC at the start.
		CalculatorCRC.Update(BufferPacket->GetRawContent(), OutgoingDefinition->GetContentSize());

		//Hash with time token as late as possible.
#ifdef USE_LATENCY_COMPENSATION
		BufferPacket->SetMACCRC(CalculatorCRC.Update(GetCryptoToken(ETTM)));
#else
		BufferPacket->SetMACCRC(CalculatorCRC.Update(GetCryptoToken(0)));
#endif		
		//

		BufferSize = AckDefinition->GetTotalSize();

		return true;
	}

	bool Setup(LoLaPacketMap* packetMap)
	{
		if (LoLaBuffer::Setup(packetMap))
		{
			AckDefinition = FindPacketDefinition(PACKET_DEFINITION_ACK_HEADER);
			AckEncryptedDefinition = FindPacketDefinition(PACKET_DEFINITION_ACK_ENCRYPTED_HEADER);
			if (AckDefinition != nullptr && AckEncryptedDefinition != nullptr)
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