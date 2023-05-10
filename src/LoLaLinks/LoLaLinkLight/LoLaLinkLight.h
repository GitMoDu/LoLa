//// LoLaLinkLight.h
//
//#ifndef _LOLA_LINK_LIGHT_h
//#define _LOLA_LINK_LIGHT_h
//
//#include <LoLaLink\LoLaPacketService.h>
//#include <LoLaLink\RollingCounters.h>
//
//template<const uint8_t MaxPacketSize, const uint8_t MaxPacketListeners = 2>
//class LoLaLinkLight : protected LoLaPacketService<MaxPacketSize>
//{
//private:
//	enum LinkStateEnum
//	{
//		NotLinked,
//		Linked
//	} LinkState = LinkStateEnum::NotLinked;
//
//	struct PacketListenerWrapper
//	{
//		ILoLaLinkPacketListener* Listener;
//		uint8_t Header = 0;
//	};
//
//	PacketListenerWrapper LinkPacketListeners[MaxPacketListeners];
//
//	static const uint8_t LoLaLightKeySize = 8;
//	uint8_t SecretKey[LoLaLightKeySize] = { 1, 2, 3, 4, 5, 6, 7, 8 };
//	BLAKE2s Hasher; // 32 Byte hasher ouput.
//
//	RollingCounters PacketCounter;
//
//	uint32_t LastValidReceived = 0;
//
//	union ArrayToUint32
//	{
//		byte array[sizeof(uint32_t)];
//		uint32_t uint;
//	} Helper;
//
//	//TODO: Turn into array, to handle multiple in and out acks.
//	uint8_t ExpectedAckHeader = 0;
//	uint8_t ExpectedAckId = 0;
//	uint8_t PendingAckHeader = 0;
//	uint8_t PendingAckId = 0;
//
//	using LoLaPacketService<MaxPacketSize>::InMessage;
//	using LoLaPacketService<MaxPacketSize>::OutMessage;
//	using LoLaPacketService<MaxPacketSize>::CanSendPacket;
//	using LoLaPacketService<MaxPacketSize>::SendPacket;
//	using LoLaPacketService<MaxPacketSize>::ReceiveTimeTimestamp;
//	using LoLaPacketService<MaxPacketSize>::PendingReceiveSize;
//
//public:
//	LoLaLinkLight(Scheduler* scheduler, ILoLaPacketDriver* driver)
//		: LoLaPacketService<MaxPacketSize>(scheduler, driver)
//		, Hasher()
//		, PacketCounter()
//	{
//		for (uint8_t i = 0; i < MaxPacketListeners; i++)
//		{
//			LinkPacketListeners[i].Listener = nullptr;
//			LinkPacketListeners[i].Header = 0;
//		}
//	}
//
//	virtual const bool Setup()
//	{
//		if (LoLaPacketService<MaxPacketSize>::Setup())
//		{
//			return true;
//		}
//
//		return false;
//	}
//
//	// LinkPacketListener Services that use:
//	//	- Packet callback.
//	//	- Ack callback.
//	// Must register here. Also included is:
//	//	- Link Status Changed callback.
//	const bool RegisterLinkPacketListener(const uint8_t header, ILoLaLinkPacketListener* listener)
//	{
//		const uint8_t count = CountLinkPacketListeners();
//
//		if (count < MaxPacketListeners)
//		{
//			// Add in the first free slot.
//			for (uint8_t i = 0; i < MaxPacketListeners; i++)
//			{
//				if (LinkPacketListeners[i].Listener == nullptr)
//				{
//					LinkPacketListeners[i].Header = header;
//					LinkPacketListeners[i].Listener = listener;
//					return true;
//				}
//			}
//		}
//
//		return false;
//	}
//
//protected:
//	// Runs when no work is pending.
//	virtual void OnService()
//	{
//		if (PendingAckHeader > 0)
//		{
//			// An ack is pending send, send ASAP!
//			SendAck(PendingAckHeader, PendingAckId);
//		}
//		else if (LinkState == LinkStateEnum::Linked &&
//			millis() - LastValidReceived > 1000)
//		{
//			LinkState = LinkStateEnum::NotLinked;
//			NotifyLink(true);
//		}
//		else if (LinkState == LinkStateEnum::Linked &&
//			millis() - LastValidReceived > 1000 / 2)
//		{
//			// TODO: Send ping with response request.
//			Task::delay(1);
//		}
//		else
//		{
//			Task::delay(1);
//		}
//	}
//
//	virtual void OnReceive() final
//	{
//		const uint8_t contentSize = PacketDefinition::GetContentSize(PendingReceiveSize);
//
//		if (DecodeInputContent(contentSize))
//		{
//			PacketCounter.PushReceivedCounter(InMessage[PacketDefinition::LOLA_PACKET_ID_INDEX]);
//
//			if (PacketCounter.CounterSkipped > 0)
//			{
//				// Packet has been accepted, notify listeners if link was acquired.
//				LastValidReceived = millis();
//
//				if (LinkState == LinkStateEnum::NotLinked)
//				{
//					LinkState = LinkStateEnum::Linked;
//					NotifyLink(true);
//				}
//
//				const uint8_t header = InMessage[PacketDefinition::LOLA_PACKET_HEADER_INDEX];
//				if (header == PacketDefinition::PACKET_DEFINITION_ACK_HEADER &&
//					ExpectedAckHeader == InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX] &&
//					ExpectedAckId == InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX + 1])
//				{
//					NotifyAck(ExpectedAckHeader, true);
//				}
//				else
//				{
//					if (NotifyPacketReceived(header, PacketDefinition::GetPayloadSizeFromContentSize(contentSize)))
//					{
//						// Listener has requested we return an ack.
//						// Sending Ack immediately.
//						// Delaying until possible if it fails.
//						SendAck(header, InMessage[PacketDefinition::LOLA_PACKET_ID_INDEX]);
//					}
//				}
//
//
//			}
//			else
//			{
//				// Replay attack prevention, packets must continuosly increase their Id counter.
//			}
//		}
//		else
//		{
//			// Packet rejected.
//		}
//	}
//
//	virtual void OnAckFailed() final
//	{
//		NotifyAck(ExpectedAckHeader, false);
//	}
//
//private:
//	const uint8_t CountLinkPacketListeners()
//	{
//		uint8_t count = 0;
//		for (uint8_t i = 0; i < MaxPacketListeners; i++)
//		{
//			if (LinkPacketListeners[i].Listener != nullptr)
//			{
//				count++;
//			}
//		}
//
//		return count;
//	}
//
//	const bool NotifyPacketReceived(const uint8_t header, const uint8_t payloadSize)
//	{
//		// Search LinkPacketListeners for this Header listener.
//		for (uint8_t i = 0; i < MaxPacketListeners; i++)
//		{
//			if (LinkPacketListeners[i].Header == header && LinkPacketListeners[i].Listener != nullptr)
//			{
//				// Registered listener found.
//				return LinkPacketListeners[i].Listener->OnPacketReceived(
//					&InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX],
//					ReceiveTimeTimestamp,
//					header,
//					payloadSize);
//			}
//		}
//
//		return false;
//	}
//
//	void NotifyLink(const bool linked)
//	{
//		// Notify all LinkPacketListeners, avoiding repition but not guaranteeing it.
//		// Picky services should delta the state, to get unique transitions.
//		ILoLaLinkPacketListener* lastListener = nullptr;
//		for (uint8_t i = 0; i < MaxPacketListeners; i++)
//		{
//			if (LinkPacketListeners[i].Listener != nullptr)
//			{
//				// Avoice notifying listeners twice, at least if they all their registries are aligned.
//				if (lastListener != LinkPacketListeners[i].Listener)
//				{
//					lastListener = LinkPacketListeners[i].Listener;
//					lastListener->OnLinkStateUpdated(linked);
//				}
//			}
//		}
//	}
//
//	void NotifyAck(const uint8_t originalHeader, const bool success)
//	{
//		// Search LinkPacketListeners for this Ack listener.
//		for (uint8_t i = 0; i < MaxPacketListeners; i++)
//		{
//			// Original header is in payload.
//			if (LinkPacketListeners[i].Header == originalHeader &&
//				LinkPacketListeners[i].Listener != nullptr)
//			{
//				if (success)
//				{
//					LinkPacketListeners[i].Listener->OnPacketAckReceived(
//						ReceiveTimeTimestamp,
//						originalHeader);
//				}
//				else
//				{
//					LinkPacketListeners[i].Listener->OnPacketAckFailed(
//						micros(),
//						originalHeader);
//				}
//				break;
//			}
//		}
//	}
//
//	// Task Service will be blocked until packet is Sent, and receives ack or nack.
//	// Concurrent requests to send will be denied.
//	// Receiving is still on.
//	const bool SendPacket(uint8_t* payload, const uint8_t payloadSize, const uint8_t header, const bool hasAck)
//	{
//		const uint8_t totalSize = PacketDefinition::GetTotalSize(payloadSize);
//
//		if (totalSize <= MaxPacketSize && CanSendPacket())
//		{
//			// Set header.
//			OutMessage[PacketDefinition::LOLA_PACKET_HEADER_INDEX] = header;
//
//			// Copy payload.
//			for (uint8_t i = 0; i < payloadSize; i++)
//			{
//				OutMessage[i + PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX] = payload[i];
//			}
//
//			// Set rolling id.
//			OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX] = PacketCounter.GetLocalNext();;
//
//			if (hasAck)
//			{
//				ExpectedAckId = OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX];
//				ExpectedAckHeader = header;
//			}
//
//			// If we are linked, encrypt packet and update mac.
//			EncodeOutputContent(PacketDefinition::GetContentSize(totalSize));
//
//			// Packet is formed and ready to attempt transmit.
//			return SendPacket(totalSize, hasAck);
//		}
//		else
//		{
//			// Turns out we can send right now, try again later.
//			return false;
//		}
//	}
//
//	void SendAck(const uint8_t originalHeader, const uint8_t originalId)
//	{
//		// Ack has 2 byte of payload.
//		// - Original header.
//		// - Original id.
//		const uint8_t totalSize = PacketDefinition::GetTotalSize(2);
//
//		// Set Ack header.
//		OutMessage[PacketDefinition::LOLA_PACKET_HEADER_INDEX] = PacketDefinition::PACKET_DEFINITION_ACK_HEADER;
//
//		// Set rolling id.
//		OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX] = PacketCounter.GetLocalNext();
//
//		// Set original header.
//		OutMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX] = originalHeader;
//
//		// Set original id.
//		OutMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX + 1] = originalId;
//
//		// If we are linked, encrypt packet and update mac.
//		EncodeOutputContent(PacketDefinition::GetContentSize(totalSize));
//
//		// Packet is formed and ready to transmit.
//		if (SendPacket(totalSize, false))
//		{
//			PendingAckHeader = 0;
//		}
//		else
//		{
//			// If not sent, store in pending and try later.
//			PendingAckHeader = originalHeader;
//			PendingAckId = originalId;
//		}
//	}
//
//	void EncodeOutputContent(const uint8_t contentSize)
//	{
//		// Reset hash.
//		Hasher.reset();
//
//		// Start with content.
//		Hasher.update(&OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX], contentSize);
//
//		// Append secret key.
//		Hasher.update(SecretKey, LoLaLightKeySize);
//
//		// Finalize hash.
//		Hasher.finalize(Helper.array, sizeof(uint32_t));
//
//		// Set MAC/CRC.
//		OutMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 0] = Helper.array[0];
//		OutMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 1] = Helper.array[1];
//		OutMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 2] = Helper.array[2];
//		OutMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 3] = Helper.array[3];
//	}
//
//	const bool DecodeInputContent(const uint8_t contentSize)
//	{
//		Helper.array[0] = InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 0];
//		Helper.array[1] = InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 1];
//		Helper.array[2] = InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 2];
//		Helper.array[3] = InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 3];
//
//		uint32_t mac = Helper.uint;
//
//		// Reset hash.
//		Hasher.reset();
//
//		// Start with content.
//		Hasher.update(&InMessage[PacketDefinition::LOLA_PACKET_ID_INDEX], contentSize);
//
//		// Append secret key.
//		Hasher.update(SecretKey, LoLaLightKeySize);
//
//		// Finalize hash.
//		Hasher.finalize(Helper.array, sizeof(uint32_t));
//
//		return mac == Helper.uint;
//	}
//};
//#endif