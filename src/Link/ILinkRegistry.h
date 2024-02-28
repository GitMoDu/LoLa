// ILinkRegistry.h

#ifndef _I_LINK_REGISTRY_h
#define _I_LINK_REGISTRY_h

#include "ILoLaLink.h"

class ILinkRegistry
{
public:
	/// <summary>
	/// Register link status listener.
	/// </summary>
	/// <param name="listener"></param>
	/// <returns>True if success. False if no more slots are available.</returns>
	virtual const bool RegisterLinkListener(ILinkListener* listener) { return false; }

	/// <summary>
	/// 
	/// </summary>
	/// <param name="listener"></param>
	/// <param name="port">Port number to register. Note that a port (upmost) may be reserved for Link.</param>
	/// <returns>True if success. False if no more slots are available or port is reserved.</returns>
	virtual const bool RegisterPacketListener(ILinkPacketListener* listener, const uint8_t port) { return false; }

	/// <summary>
	/// Notify services that link is ready/lost.
	/// </summary>
	/// <param name="hasLink"></param>
	virtual void NotifyLinkListeners(const bool hasLink) { }

	/// <summary>
	/// Notify the port-registered service of incoming packet.
	/// </summary>
	/// <param name="timestamp"></param>
	/// <param name="payload"></param>
	/// <param name="payloadSize"></param>
	/// <param name="port"></param>
	virtual void NotifyPacketListener(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) { }
};

template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LinkRegistry : public ILinkRegistry
{
private:
	struct PacketListenerWrapper
	{
		ILinkPacketListener* Listener = nullptr;
		uint8_t Port = 0;
	};

private:
	PacketListenerWrapper LinkPacketListeners[MaxPacketReceiveListeners];
	ILinkListener* LinkListeners[MaxLinkListeners];
	uint8_t PacketListenersCount = 0;
	uint8_t LinkListenersCount = 0;

public:
	LinkRegistry()
		: LinkPacketListeners()
		, LinkListeners()
	{
	}

	const bool Setup()
	{
		return MaxPacketReceiveListeners >= LoLaLinkDefinition::UN_LINKED_PORT_ALLOCATION;
	}

	/// <summary>
	/// Register link status listener.
	/// </summary>
	/// <param name="listener"></param>
	/// <returns>True if success. False if no more slots are available.</returns>
	virtual const bool RegisterLinkListener(ILinkListener* listener) final
	{
		if (listener != nullptr && LinkListenersCount < MaxLinkListeners - 1)
		{
			for (uint_fast8_t i = 0; i < LinkListenersCount; i++)
			{
				if (LinkListeners[i] == listener)
				{
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

	/// <summary>
	/// </summary>
	/// <param name="listener"></param>
	/// <returns>True if success. False if no more slots are available.</returns>
	virtual const bool RegisterPacketListener(ILinkPacketListener* listener, const uint8_t port) final
	{
		if (listener != nullptr
			&& (PacketListenersCount < MaxPacketReceiveListeners))
		{
			for (uint_fast8_t i = 0; i < PacketListenersCount; i++)
			{
				if (LinkPacketListeners[i].Port == port)
				{
#if defined(DEBUG_LOLA)
					Serial.print(F("Port "));
					Serial.print(port);
					Serial.println(F(" already registered."));
#endif
					return false;
				}
			}

			LinkPacketListeners[PacketListenersCount].Listener = listener;
			LinkPacketListeners[PacketListenersCount].Port = port;
			PacketListenersCount++;

			return true;
		}
		else
		{
			return false;
		}
	}

	virtual void NotifyLinkListeners(const bool hasLink) final
	{
		for (uint_fast8_t i = 0; i < LinkListenersCount; i++)
		{
			LinkListeners[i]->OnLinkStateUpdated(hasLink);
		}
	}

	virtual void NotifyPacketListener(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port) final
	{
		for (uint_fast8_t i = 0; i < PacketListenersCount; i++)
		{
			if (port == LinkPacketListeners[i].Port)
			{
				LinkPacketListeners[i].Listener->OnPacketReceived(
					timestamp,
					payload,
					payloadSize,
					port);

				break;
			}
		}
	}
};
#endif