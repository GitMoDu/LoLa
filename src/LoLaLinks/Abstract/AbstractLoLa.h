// AbstractLoLa.h

#ifndef _ABSTRACT_LOLA_
#define _ABSTRACT_LOLA_




#include "..\..\Link\ILoLaLink.h"
#include "..\..\Service\TemplateLoLaService.h"

/// <summary>
/// LoLa Link base is a special case of a LoLaService,
/// it will handle tx/rx pre-link packets as well as use plain link time tx/rx for link upkeep.
/// As a partial abstract class, it implements the following ILoLaLink calls:
///		- Link Listeners.
///		- Receive Listeners.
///		- Link state and virtual update.
/// </summary>
template<const uint8_t MaxPayloadLinkSend,
	const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLa : public TemplateLoLaService<MaxPayloadLinkSend>
	, public virtual ILoLaLink
{
private:
	using BaseClass = TemplateLoLaService<MaxPayloadLinkSend>;

	struct LinkPacketServiceWrapper
	{
		ILinkPacketReceiver* Listener = nullptr;
		uint8_t Port = 0;
	};

protected:
	LinkPacketServiceWrapper LinkPacketServiceListeners[MaxPacketReceiveListeners];
	uint8_t PacketServiceListenersCount = 0;

	ILinkListener* LinkListeners[MaxLinkListeners];
	uint8_t LinkListenersCount = 0;

	// The outgoing content is encrypted and MAC'd here before being sent to the driver for transmission.
	uint8_t RawOutPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	// The incoming encrypted and MAC'd packet is stored, here before validated, and decrypted to InData.
	uint8_t RawInPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	// The incoming plaintext content is decrypted to here, from the RawInPacket.
	uint8_t InData[LoLaPacketDefinition::GetDataSize(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)]{};

#if defined(DEBUG_LOLA)
	virtual void Owner() {}
#endif

public:
	AbstractLoLa(Scheduler& scheduler)
		: BaseClass(scheduler, this)
		, ILoLaLink()
		, LinkPacketServiceListeners()
		, LinkListeners()
	{}

public:
	virtual const bool RegisterLinkListener(ILinkListener* listener) final
	{
		if (listener != nullptr && LinkListenersCount < MaxLinkListeners - 1)
		{
			for (uint_fast8_t i = 0; i < LinkListenersCount; i++)
			{
				if (LinkListeners[i] == listener)
				{
					// Listener already registered.
#if defined(DEBUG_LOLA)
					Serial.println(F("Listener already registered."));
#endif
					return false;
				}
			}

			LinkListeners[LinkListenersCount] = listener;
			LinkListenersCount++;

			return true;
		}
		return false;
	}

protected:
	void PostLinkState(const bool hasLink)
	{
		for (uint_fast8_t i = 0; i < LinkListenersCount; i++)
		{
			LinkListeners[i]->OnLinkStateUpdated(hasLink);
		}
	}

	void NotifyPacketReceived(const uint32_t startTimestamp, const uint8_t port, const uint8_t payloadSize)
	{
		for (uint_fast8_t i = 0; i < PacketServiceListenersCount; i++)
		{
			if (port == LinkPacketServiceListeners[i].Port)
			{
				LinkPacketServiceListeners[i].Listener->OnPacketReceived(startTimestamp,
					&InData[LoLaPacketDefinition::PAYLOAD_INDEX - LoLaPacketDefinition::DATA_INDEX],
					payloadSize,
					port);

				break;
			}
		}
	}

	const bool RegisterPacketReceiverInternal(ILinkPacketReceiver* listener, const uint8_t port)
	{
		if (listener != nullptr
			&& (PacketServiceListenersCount < MaxPacketReceiveListeners))
		{
			for (uint_fast8_t i = 0; i < PacketServiceListenersCount; i++)
			{
				if (LinkPacketServiceListeners[i].Port == port)
				{
					// Port already registered.
#if defined(DEBUG_LOLA)
					Serial.print(F("Port "));
					Serial.print(port);
					Serial.println(F(" already registered."));
#endif
					return false;
				}
			}

			LinkPacketServiceListeners[PacketServiceListenersCount].Listener = listener;
			LinkPacketServiceListeners[PacketServiceListenersCount].Port = port;
			PacketServiceListenersCount++;

			return true;
		}
		else
		{
			return false;
		}
	}	
};
#endif