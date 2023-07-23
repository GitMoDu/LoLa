// AbstractLoLa.h

#ifndef _ABSTRACT_LOLA_
#define _ABSTRACT_LOLA_


#include "..\..\Service\TemplateLoLaService.h"

#include "..\..\Link\LinkStageEnum.h"
#include "..\..\Link\PacketEventEnum.h"

#include "..\..\Link\LoLaPacketService.h"

#include "..\..\Clock\SynchronizedClock.h"

#include "..\..\Link\ILoLaLink.h"
#include "..\..\Link\LoLaPacketDefinition.h"
#include "..\..\Crypto\LoLaCryptoEncoderSession.h"

/// <summary>
/// LoLa Link base (AbstractLoLa) is a special case of a TemplateLoLaService,
/// as it will handle pre-link Packet as well as use link time packets for link upkeep.
/// As a partial abstract class, it implements the following ILoLaLink calls:
///		- Link Listeners.
///		- Receive Listeners.
///		- Link state and virtual update.
/// </summary>
template<const uint8_t MaxPayloadLinkSend,
	const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLa : protected TemplateLoLaService<MaxPayloadLinkSend>
	, public virtual ILoLaLink
	, public virtual IPacketServiceListener
{
private:
	using BaseClass = TemplateLoLaService<MaxPayloadLinkSend>;

	struct LinkPacketServiceWrapper
	{
		ILinkPacketListener* Listener = nullptr;
		uint8_t Port = 0;
	};

protected:
	// Static value with all zeros.
	Timestamp ZeroTimestamp{};

protected:
	LinkPacketServiceWrapper LinkPacketServiceListeners[MaxPacketReceiveListeners];
	uint8_t PacketServiceListenersCount = 0;

	ILinkListener* LinkListeners[MaxLinkListeners]{};
	uint8_t LinkListenersCount = 0;

	// Packet service instance, templated to max packet size and reference low latency timeouts.
	LoLaPacketService<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE> PacketService;

	// The outgoing content is encrypted and MAC'd here before being sent to the Transceiver for transmission.
	uint8_t RawOutPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	// The incoming encrypted and MAC'd packet is stored, here before validated, and decrypted to InData.
	uint8_t RawInPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	// The incoming plaintext content is decrypted to here, from the RawInPacket.
	uint8_t InData[LoLaPacketDefinition::GetDataSize(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)]{};

protected:
	// Rx/Tx Transceiver for PHY.
	ILoLaTransceiver* Transceiver;

	// Expandable session encoder.
	LoLaCryptoEncoderSession* Encoder;

protected:
	// Duplex, Channel Hop and Cryptography depend on a synchronized clock between Server and Client.
	SynchronizedClock SyncClock;

	// Current Link Stage for packet handling.
	volatile LinkStageEnum LinkStage = LinkStageEnum::Disabled;

protected:
	virtual void OnEvent(const PacketEventEnum packetEvent) {}

#if defined(DEBUG_LOLA)
protected:
	virtual void Owner() {}

	size_t print(const __FlashStringHelper* ifsh) { return print(reinterpret_cast<const char*>(ifsh)); }

	void Skipped(const __FlashStringHelper* ifsh)
	{
		return Skipped(reinterpret_cast<const char*>(ifsh));
	}

	void Skipped(const String& label)
	{
		this->Owner();
		Serial.print(F("Rejected "));
		Serial.println(label);
	}
#endif


public:
	AbstractLoLa(Scheduler& scheduler,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		IClockSource* clockSource,
		ITimerSource* timerSource)
		: ILoLaLink()
		, IPacketServiceListener()
		, BaseClass(scheduler, this)
		, LinkPacketServiceListeners()
		, PacketService(scheduler, this, transceiver, RawInPacket, RawOutPacket)
		, Transceiver(transceiver)
		, Encoder(encoder)
		, SyncClock(clockSource, timerSource)
	{}

	virtual const bool Setup()
	{
		if (Transceiver == nullptr)
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("Transceiver not found."));
#endif
			return false;
		}

		if (!SyncClock.Setup())
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("SyncClock setup failed."));
#endif
			return false;
		}

		if (!PacketService.Setup())
		{
#if defined(DEBUG_LOLA)
			Serial.println(F("PacketService setup failed."));
#endif
			return false;
		}

		return true;
	}

public:
	/// <summary>
	/// Overridable callback.
	/// </summary>
	/// <param name="result">SendResultEnum</param>
	virtual void OnSendComplete(const IPacketServiceListener::SendResultEnum result)
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

	const bool RegisterPacketReceiverInternal(ILinkPacketListener* listener, const uint8_t port)
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