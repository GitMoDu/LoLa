// LoLaPacketLink.h

#ifndef _LOLA_PACKET_LINK_h
#define _LOLA_PACKET_LINK_h


#include <LoLaLink\LoLaPacketCrypto.h>


template<const uint8_t MaxPacketSize>
class LoLaPacketLink : protected LoLaPacketCrypto<MaxPacketSize>
{
protected:
	using LoLaPacketCrypto<MaxPacketSize>::InMessage;
	using LoLaPacketCrypto<MaxPacketSize>::OutMessage;
	using LoLaPacketCrypto<MaxPacketSize>::SendPacket;
	using LoLaPacketCrypto<MaxPacketSize>::DecodeInputContent;
	using LoLaPacketCrypto<MaxPacketSize>::EncodeOutputContent;

private:
	uint8_t ExpectedAckId = 0;

public:
	LoLaPacketLink(Scheduler* scheduler, ILoLaPacketDriver* driver)
		: LoLaPacketCrypto<MaxPacketSize>(scheduler, driver)
	{
	}

	virtual const bool Setup()
	{
		if (LoLaPacketCrypto<MaxPacketSize>::Setup())
		{
			return true;
		}

		return false;
	}

protected:
	// To be overriden by link.
	virtual void OnLinkReceiveAck() { }
	virtual void OnLinkReceiveNack() { }
	virtual void OnLinkReceive(const uint8_t contentSize) { }

	virtual void NotifyAckReceived(const uint8_t originalHeader) { }
	virtual void NotifyNackReceived(const uint8_t originalHeader) { }
	virtual void NotifyPacketReceived(const uint8_t header, const uint8_t payloadSize) { }

protected:
	virtual void OnReceive(const uint8_t contentSize)
	{
		//// Get MAC key.
		const uint32_t token = 0;

		//if (TokenHopEnabled)
		//{
		//	token = LinkTokenGenerator.GetTokenFromAhead(forwardsMillis);
		//}
		//else
		//{
		//	token = LinkTokenGenerator.GetSeed();
		//}

		// If linked, decrypt content.
		if (DecodeInputContent(token, contentSize))
		{
			// Packet refused on decoding.
			return;
		}

		const uint8_t header = InMessage[PacketDefinition::LOLA_PACKET_HEADER_INDEX];

		// Handle reserved link headers first, then fall through normal operation.
		switch (header)
		{
		case PacketDefinition::PACKET_DEFINITION_LINK_START_HEADER ... PacketDefinition::PACKET_DEFINITION_LINK_END_HEADER:
			OnLinkReceive(contentSize);
			break;
		case PacketDefinition::PACKET_DEFINITION_ACK_HEADER:
			// Does the ack id match our expected id?
			if (ExpectedAckId == InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX + 1])
			{
				// Handle reserved link acks first, then fall through normal operation.
				if (InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX] > PacketDefinition::PACKET_DEFINITION_LINK_START_HEADER - 1)
				{
					OnLinkReceiveAck();
				}
				else
				{
					NotifyAckReceived(InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX]);
				}
			}
			else
			{
				// Unexpected Ack Id.
				//TODO: log to statistics.
				break;
			}
			break;
		default:
			NotifyPacketReceived(header, PacketDefinition::GetPayloadSizeFromContentSize(contentSize));
			break;
		}
	}

	// Task Service will be blocked until packet is Sent, and receives ack or nack.
	// Concurrent requests to send will be denied.
	// Receiving is still on.
	virtual const bool SendPacket(uint8_t* payload, const uint8_t payloadSize, const uint8_t header, const bool hasAck)
	{
		const uint8_t totalSize = PacketDefinition::GetTotalSize(payloadSize);

		if (totalSize <= MaxPacketSize && CanSendPacket())
		{
			// Set header.
			OutMessage[PacketDefinition::LOLA_PACKET_HEADER_INDEX] = header;

			// Copy payload.
			for (uint8_t i = 0; i < payloadSize; i++)
			{
				OutMessage[i + PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX] = payload[i];
			}

			// Set rolling id.
			SetOutId();

			if (hasAck)
			{
				ExpectedAckId = OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX];
			}

			// Assume we're ETTM in the future,
			// to remove transmition duration from token clock jitter.
			const uint8_t ettm = 0;
			const uint32_t token = 0;

			// If we are linked, encrypt packet and update mac.
			EncodeOutputContent(token, PacketDefinition::GetContentSize(totalSize));

			// Packet is formed and ready to attempt transmit.
			return SendPacket(totalSize, hasAck);
		}
		else
		{
			// Turns out we can send right now, try again later.
			return false;
		}
	}

	virtual void OnAckFailed()
	{
		// Handle reserved link acks first, then fall through normal operation.
		if (ExpectedAckId > PacketDefinition::PACKET_DEFINITION_LINK_START_HEADER - 1)
		{
			OnLinkReceiveNack();
		}
		else
		{
			NotifyNackReceived(ExpectedAckId);
		}
	}
};
#endif

