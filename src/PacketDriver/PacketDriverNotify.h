// PacketDriverNotify.h

#ifndef _PACKETDRIVERNOTIFY_h
#define _PACKETDRIVERNOTIFY_h

#include <LoLaDefinitions.h>
#include <Packet\LoLaPacket.h>

class NotifiableAckReverseDefinition
{
private:
	PacketDefinition* OriginalDefinition = nullptr;
	PacketDefinition* AckDefinition = nullptr;

public:
	bool SetOriginalDefinition(PacketDefinition* originalDefinition)
	{
		// If definition exists and is not recursively pointing to Ack.
		if (originalDefinition != nullptr &&
			originalDefinition != AckDefinition)
		{
			OriginalDefinition = originalDefinition;

			return true;
		}
		else
		{
			return false;
		}
	}

	bool SetAckDefinition(PacketDefinition* ackDefinition)
	{
		AckDefinition = ackDefinition;

		return AckDefinition != nullptr;
	}

	void NotifyAckReceived(const uint8_t id, const uint32_t timestamp)
	{
		OriginalDefinition->Service->OnPacketTransmited(OriginalDefinition->Header, id, timestamp);
	}
};

class NotifiableIncomingPacket : public TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>
{
public:
	bool NotifyPacketReceived(const uint32_t timestamp)
	{

		return GetDefinition()->Service->OnPacketReceived(GetDefinition()->Header, GetId(), timestamp, GetPayload());
	}
};

class NotifiableOutgoingPacket : public TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>
{
public:
	void NotifyPacketTransmitted(const uint8_t id, const uint32_t timestamp)
	{
		TemplateLoLaPacket<LOLA_PACKET_MAX_PACKET_SIZE>::GetDefinition()->Service->OnPacketTransmited(GetDefinition()->Header, id, timestamp);
	}
};


#endif

