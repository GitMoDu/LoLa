// AbstractLoLa.h

#ifndef _ABSTRACT_LOLA_
#define _ABSTRACT_LOLA_


#include "..\..\Link\ILoLaLink.h"
#include "..\..\Link\ILinkRegistry.h"
#include "..\..\Link\LoLaLinkDefinition.h"

#include "..\..\Link\PacketEventEnum.h"
#include "..\..\Link\LoLaPacketService.h"

#include "..\..\Clock\LinkClock.h"
#include "..\..\Crypto\LoLaCryptoEncoderSession.h"

#include "..\..\Services\Template\TemplateLinkService.h"

/// <summary>
/// LoLa Link base (AbstractLoLa) is a special case of a TemplateLinkService,
/// as it will handle pre-link Packet as well as use link time packets for link upkeep.
/// As a partial abstract class, it implements ILoLaLink Listeners public register.
/// </summary>
class AbstractLoLa : protected TemplateLinkService<LoLaPacketDefinition::MAX_PAYLOAD_SIZE>
	, public virtual ILoLaLink
	, public virtual IPacketServiceListener
{
private:
	using BaseClass = TemplateLinkService<LoLaPacketDefinition::MAX_PAYLOAD_SIZE>;

	struct PacketListenerWrapper
	{
		ILinkPacketListener* Listener = nullptr;
		uint8_t Port = 0;
	};

protected:
	enum class LinkStageEnum : uint8_t
	{
		Disabled,
		Booting,
		AwaitingLink,
		Linking,
		Linked
	};

protected:
	// Packet service instance;
	LoLaPacketService PacketService;

	// Listener registry interface.
	ILinkRegistry* Registry;

	// The outgoing content is encrypted and MAC'd here before being sent to the Transceiver for transmission.
	uint8_t RawOutPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	// The incoming encrypted and MAC'd packet is stored here before validated, and decrypted to InData.
	uint8_t RawInPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

protected:
	// Rx/Tx Transceiver for PHY.
	ILoLaTransceiver* Transceiver;

	// Expandable session encoder.
	LoLaCryptoEncoderSession* Encoder;

protected:
	// Duplex, Channel Hop and Cryptography depend on a synchronized clock between Server and Client.
	LinkClock SyncClock;

	// Current Link Stage for packet handling.
	volatile LinkStageEnum LinkStage = LinkStageEnum::Disabled;

protected:
	virtual void OnEvent(const PacketEventEnum packetEvent) {}

#if defined(DEBUG_LOLA) || defined(DEBUG_LOLA_LINK)
protected:
	virtual void Owner() {}
#endif

#if defined(LINK_TEST_DETUNE)
public:
	TuneClock* GetInternalClock()
	{
		return &SyncClock;
	}
#endif

public:
	AbstractLoLa(Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		ICycles* cycles)
		: ILoLaLink()
		, IPacketServiceListener()
		, BaseClass(scheduler, this)
		, PacketService(scheduler, this, transceiver, RawInPacket, RawOutPacket)
		, Registry(linkRegistry)
		, Transceiver(transceiver)
		, Encoder(encoder)
		, SyncClock(scheduler, cycles)
	{}

public:
	virtual const bool Setup()
	{
		if (!BaseClass::Setup())
		{
			return false;
		}

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
	const bool RegisterLinkListener(ILinkListener* listener) final
	{
		return Registry->RegisterLinkListener(listener);
	}

	const bool RegisterPacketListener(ILinkPacketListener* listener, const uint8_t port) final
	{
		if (port <= LoLaLinkDefinition::MAX_DEFINITION_PORT)
		{
			return Registry->RegisterPacketListener(listener, port);
		}
		return false;
	}

protected:
	static const uint32_t ArrayToUInt32(const uint8_t* source)
	{
		uint32_t value = source[0];
		value |= (uint32_t)source[1] << 8;
		value |= (uint32_t)source[2] << 16;
		value |= (uint32_t)source[3] << 24;

		return value;
	}

	static void UInt32ToArray(const uint32_t value, uint8_t* target)
	{
		target[0] = value;
		target[1] = value >> 8;
		target[2] = value >> 16;
		target[3] = value >> 24;
	}

	static void Int32ToArray(const int32_t value, uint8_t* target)
	{
		UInt32ToArray(value, target);
	}

	static const int32_t ArrayToInt32(const uint8_t* source)
	{
		return (int32_t)ArrayToUInt32(source);
	}

#if defined(DEBUG_LOLA) || defined(DEBUG_LOLA_LINK)
protected:
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
};
#endif