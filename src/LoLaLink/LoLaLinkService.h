// LoLaLinkService.h

#ifndef _LOLA_LINK_SERVICE_h
#define _LOLA_LINK_SERVICE_h


#include <LoLaLink\LoLaPacketLink.h>

template<const uint8_t MaxPacketSize, const uint8_t MaxLinkListeners = 4, const uint8_t MaxPacketListeners = 8>
class LoLaLinkService : protected LoLaPacketLink<MaxPacketSize>, public virtual ILoLaLink
{
private:
	struct PacketListenerWrapper
	{
		ILoLaLinkPacketListener* Listener;
		uint8_t Header = 0;
	};

	PacketListenerWrapper LinkPacketListeners[MaxPacketListeners];

	ILoLaLinkListener* LinkListeners[MaxLinkListeners];

protected:
	using LoLaPacketLink<MaxPacketSize>::InMessage;
	using LoLaPacketLink<MaxPacketSize>::OutMessage;
	using LoLaPacketLink<MaxPacketSize>::ReceiveTimeTimestamp;
	using LoLaPacketLink<MaxPacketSize>::CanSendPacket;
	using LoLaPacketLink<MaxPacketSize>::SendPacket;

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
	virtual void NotifyPacketReceived(const uint8_t header, const uint8_t payloadSize) {// Search LinkPacketListeners for this Header listener.
		for (uint8_t i = 0; i < MaxPacketListeners; i++)
		{
			if (LinkPacketListeners[i].Header = header && LinkPacketListeners[i].Listener != nullptr)
			{
				// Registered listener found.
				if (LinkPacketListeners[i].Listener->OnPacketReceived(
					&InMessage[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX],
					ReceiveTimeTimestamp,
					header,
					payloadSize))
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
	}

	virtual void NotifyAckReceived(const uint8_t originalHeader)
	{
		// Search LinkPacketListeners for this Ack listener.
		for (uint8_t i = 0; i < MaxPacketListeners; i++)
		{
			// Original header is in payload.
			if (LinkPacketListeners[i].Header = originalHeader &&
				LinkPacketListeners[i].Listener != nullptr)
			{
				LinkPacketListeners[i].Listener->OnPacketAckReceived(
					ReceiveTimeTimestamp,
					originalHeader);
				break;
			}
		}
	}


public:
	LoLaLinkService(Scheduler* scheduler, ILoLaPacketDriver* driver)
		: LoLaPacketLink<MaxPacketSize>(scheduler, driver)
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

	virtual const bool Setup()
	{
		if (LoLaPacketLink<MaxPacketSize>::Setup())
		{
			return true;
		}

		return false;
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

protected:
	virtual void NotifyLinkStateUpdate(const bool linked)
	{
		// Notify all packet listeners.
		for (uint8_t i = 0; i < MaxPacketListeners; i++)
		{
			if (LinkPacketListeners[i].Listener != nullptr)
			{
				LinkPacketListeners[i].Listener->OnLinkStateUpdated(linked);
			}
		}

		// Notify all link listeners.
		for (uint8_t i = 0; i < MaxLinkListeners; i++)
		{
			if (LinkListeners[i] != nullptr)
			{
				LinkListeners[i]->OnLinkStateUpdated(linked);
			}
		}
	}

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