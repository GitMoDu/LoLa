// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h


#include <Link\LoLaPacketService.h>


template<const uint8_t MaxPacketSize, const uint8_t MaxLinkListeners = 4, const uint8_t MaxPacketListeners = 8>
class LoLaLinkService : public LoLaCryptoSendReceive<MaxPacketSize>, public virtual ILoLaLink
{
private:
	// Link report tracking.
	//bool ReportPending = false;

public:
	//// Shared Sub state helpers.
	//enum LinkingStagesEnum
	//{
	//	ChallengeStage = 0,
	//	ClockSyncStage = 1,
	//	LinkProtocolSwitchOver = 2,
	//	LinkingDone = 3
	//} LinkingState = LinkingStagesEnum::ChallengeStage;

	//enum ProcessingDHEnum
	//{
	//	GeneratingSecretKey = 0,
	//	ExpandingKey = 1
	//} ProcessingDHState = ProcessingDHEnum::GeneratingSecretKey;


	ILoLaLinkListener* LinkListeners[MaxLinkListeners];

	struct PacketListenerWrapper
	{
		ILoLaLinkPacketListener* Listener;
		uint8_t Header = 0;
	};
	PacketListenerWrapper LinkPacketListeners[MaxPacketListeners];

	using LoLaCryptoSendReceive<MaxPacketSize>::InMessage;
	using LoLaCryptoSendReceive<MaxPacketSize>::OutMessage;
	using LoLaCryptoSendReceive<MaxPacketSize>::ReceiveTimeTimestamp;
	using LoLaCryptoSendReceive<MaxPacketSize>::CanSendPacket;
	using LoLaCryptoSendReceive<MaxPacketSize>::SendPacket;
	using LoLaCryptoSendReceive<MaxPacketSize>::DecodeInputContent;
	using LoLaCryptoSendReceive<MaxPacketSize>::EncodeOutputContent;

	uint8_t ExpectedAckId = 0;


private:
	const uint8_t CountLinkListeners()
	{
		uint8_t count = 0;
		for (uint8_t i = 0; i < MaxLinkListeners; i++)
		{
			if (LinkListeners[i] != nullptr)
			{
				count++;
			}
		}

		return count;
	}

	const uint8_t CountLinkPacketListeners()
	{
		uint8_t count = 0;
		for (uint8_t i = 0; i < MaxPacketListeners; i++)
		{
			if (LinkPacketListeners[i].Listener != nullptr)
			{
				count++;
			}
		}

		return count;

	}

protected:
	// To be overriden.
	virtual void OnLinkReceiveAck(const uint8_t header) { }
	virtual void OnLinkReceiveNack(const uint8_t header) { }
	virtual void OnLinkReceive(const uint8_t header, const uint8_t contentSize) { }

public:
	LoLaLinkService(Scheduler* scheduler, ILoLaPacketDriver* driver)
		: LoLaCryptoSendReceive<MaxPacketSize>(scheduler, driver)
	{
		for (uint8_t i = 0; i < MaxLinkListeners; i++)
		{
			LinkListeners[i] = nullptr;
		}
		for (uint8_t i = 0; i < MaxPacketListeners; i++)
		{
			LinkPacketListeners[i].Listener = nullptr;
			LinkPacketListeners[i].Header = 0;
		}
	}

	const bool RegisterLinkListener(ILoLaLinkListener* listener)
	{
		const uint8_t count = CountLinkListeners();

		if (count < MaxLinkListeners)
		{
			// Add in the first free slot.
			for (uint8_t i = 0; i < MaxLinkListeners; i++)
			{
				if (LinkListeners[i] == nullptr)
				{
					LinkListeners[i] = listener;
					return true;
				}
			}
		}

		return false;
	}

	const bool RegisterLinkPacketListener(const uint8_t header, ILoLaLinkPacketListener* listener)
	{
		const uint8_t count = CountLinkPacketListeners();

		if (count < MaxPacketListeners)
		{
			// Add in the first free slot.
			for (uint8_t i = 0; i < MaxPacketListeners; i++)
			{
				if (LinkPacketListeners[i].Listener == nullptr)
				{
					LinkPacketListeners[i].Header = header;
					LinkPacketListeners[i].Listener = listener;
					return true;
				}
			}
		}

		return false;
	}

	virtual const bool Setup()
	{
		if (LoLaCryptoSendReceive<MaxPacketSize>::Setup())
		{
			//ClearSession();
			//ResetStateStartTime();

			return true;

		}

		return false;
	}

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
			OnLinkReceive(header, contentSize);
			break;
		case PacketDefinition::PACKET_DEFINITION_ACK_HEADER:
			// Does the ack id match our expected id?
			if (ExpectedAckId == InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX + 1])
			{
				// Handle reserved link acks first, then fall through normal operation.
				if (InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX] > PacketDefinition::PACKET_DEFINITION_LINK_START_HEADER - 1)
				{
					OnLinkReceiveAck(header);
				}
				else
				{
					// Search LinkPacketListeners for this Ack listener.
					for (uint8_t i = 0; i < MaxPacketListeners; i++)
					{
						// Original header is in payload.
						if (LinkPacketListeners[i].Header = InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX] &&
							LinkPacketListeners[i].Listener != nullptr)
						{
							LinkPacketListeners[i].Listener->OnPacketAckReceived(
								ReceiveTimeTimestamp,
								InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX]);
							break;
						}
					}
				}
			}
			else
			{
				// Unexpected Ack Id.
				//TODO: log to statistics.
				OnAckFailed();
				break;
			}
			break;
		default:
			// Search LinkPacketListeners for this Header listener.
			for (uint8_t i = 0; i < MaxPacketListeners; i++)
			{
				if (LinkPacketListeners[i].Header = header && LinkPacketListeners[i].Listener != nullptr)
				{
					// Registered listener found.
					if (LinkPacketListeners[i].Listener->OnPacketReceived(
						&InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX],
						ReceiveTimeTimestamp,
						header,
						PacketDefinition::GetPayloadSizeFromContentSize(contentSize)))
					{
						// Listener has requested we return an ack.
						// Sending Ack immediately, without check.
						// TODO: delaying until possible might work,
						// but then the receiver should know the ack was delayed.
						SendAck(header, InMessage[PacketDefinition::LOLA_PACKET_ID_INDEX]);
						break;
					}
				}
			}
			break;
		}
	}

	// Task Service will be blocked until packet is Sent, and receives ack or nack.
	// Concurrent requests to send will be denied.
	// Receiving is still on.
	const bool SendPacket(uint8_t* payload, const uint8_t payloadSize, const uint8_t header, const bool hasAck)
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

	/*void Start()
	{
		Driver->DriverStart();
		UpdateLinkState(LinkStateEnum::AwaitingLink);
	}

	void Stop()
	{
		UpdateLinkState(LinkStateEnum::Disabled);
	}*/

private:
	void SendAck(const uint8_t originalHeader, const uint8_t originalId)
	{
		// Ack has 2 byte of payload.
		// - Original header.
		// - Original id.
		const uint8_t totalSize = PacketDefinition::GetTotalSize(2);

		// Set Ack header.
		OutMessage[PacketDefinition::LOLA_PACKET_HEADER_INDEX] = PacketDefinition::PACKET_DEFINITION_ACK_HEADER;

		// Set original header.
		OutMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX] = originalHeader;

		// Set original id.
		OutMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX + 1] = originalId;

		// Set rolling id.
		SetOutId();

		// Assume we're ETTM in the future,
		// to remove transmition duration from token clock jitter.
		const uint8_t ettm = 0;
		const uint32_t token = 0;

		// If we are linked, encrypt packet and update mac.
		EncodeOutputContent(token, PacketDefinition::GetContentSize(totalSize));

		// Packet is formed and ready to transmit.
		// Ignoring send fails.
		SendPacket(totalSize, false);
	}
};
#endif