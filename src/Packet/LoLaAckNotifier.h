// NotifiablePacket.h

#ifndef _NOTIFIABLE_PACKET_h
#define _NOTIFIABLE_PACKET_h

#include <LoLaDefinitions.h>
#include <Packet\LoLaPacket.h>

class LoLaAckNotifier
{
private:
	LoLaPacketMap* PacketMap = nullptr;
	PacketDefinition* AckDefinition = nullptr;

public:
	LoLaAckNotifier(LoLaPacketMap* packetMap)
		: PacketMap(packetMap)
	{
		AckDefinition = PacketMap->GetAckDefinition();
	}

	const bool IsAckHeader(const uint8_t header)
	{
		return header == AckDefinition->Header;
	}

	void NotifyAckReceived(const uint8_t originalHeader, const uint8_t originalId, const uint32_t timestamp)
	{
		// Get the reverse definition from the originalHeader.
		PacketDefinition* Definition = PacketMap->GetDefinitionNotAck(originalHeader);

		if (Definition != nullptr)
		{
			Definition->Service->OnAckReceived(originalHeader, originalId, timestamp);
		}
		else
		{
			Serial.print("NotifyAckReceived Not found: ");
			Serial.println(originalHeader);

		}
	}
};

#endif

