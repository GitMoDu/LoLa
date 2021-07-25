// AbstractLinkService.h

#ifndef _LOLA_ABSTRACT_LINK_SERVICE_h
#define _LOLA_ABSTRACT_LINK_SERVICE_h

#include <Services\LoLaPacketService.h>
#include <Link\LoLaLinkDefinitions.h>
#include <Link\ProtocolVersionCalculator.h>

#include <Link\LoLaLinkInfo.h>

#include <LoLaCrypto\LoLaCryptoKeyExchanger.h>
#include <LoLaCrypto\LoLaCryptoEncoder.h>


class AbstractLinkService : public LoLaPacketService<LinkPackets::MAX_PAYLOAD_SIZE, false>
{
protected:
	enum LinkStateEnum
	{
		Disabled = 0,
		AwaitingLink = 1,
		AwaitingSleeping = 2,
		Linking = 3,
		Linked = 4
	};

private:
	// Runtime helpers.
	uint32_t StateStartTime = 0;

	// Packet definitions.
	LinkPKERequestPacketDefinition		DefinitionPKERequest;
	LinkPKEResponsePacketDefinition		DefinitionPKEResponse;
	LinkStartWithAckPacketDefinition	DefinitionStartLink;

	LinkChallengePacketDefinition		DefinitionChallenge;
	LinkClockSyncPacketDefinition		DefinitionClockSync;
	LinkReportPacketDefinition			DefinitionReport;

protected:
	// Link Info.
	LoLaLinkInfo* LinkInfo;

	// PacketIO Info.
	PacketIOInfo* IOInfo;

	// Host/Remote clock syncer.
	LoLaLinkClockSyncer* ClockSyncer;

	// Crypto key exchanger.
	LoLaCryptoKeyExchanger KeyExchanger;

	// State.
	LinkStateEnum LinkState = LinkStateEnum::Disabled;

public:
	AbstractLinkService(Scheduler* scheduler
		, LoLaPacketDriver* driver
		, LoLaLinkClockSyncer* clockSyncer)
		: LoLaPacketService(scheduler, LOLA_LINK_SERVICE_CHECK_PERIOD, driver)
		, DefinitionPKERequest(this)
		, DefinitionPKEResponse(this)
		, DefinitionStartLink(this)
		, DefinitionChallenge(this)
		, DefinitionClockSync(this)
		, DefinitionReport(this)
		, LinkInfo(&driver->LinkInfo)
		, IOInfo(&driver->IOInfo)
		, KeyExchanger()
		, ClockSyncer(clockSyncer)
	{
	}

	virtual bool Setup()
	{
		KeyExchanger.Setup();
		//TODO: Replace with constant random address, bitcoin style.
		// the public key is the address.
		//TODO: Sign the address with the certificate.
		KeyExchanger.GenerateNewKeyPair();

#ifdef DEBUG_LOLA
		// Make sure our re-usable packet has enough space.
		if (!ValidateMessageSize(DefinitionPKERequest.PayloadSize) ||
			!ValidateMessageSize(DefinitionPKEResponse.PayloadSize) ||
			!ValidateMessageSize(DefinitionStartLink.PayloadSize) ||
			!ValidateMessageSize(DefinitionChallenge.PayloadSize) ||
			!ValidateMessageSize(DefinitionClockSync.PayloadSize) ||
			!ValidateMessageSize(DefinitionReport.PayloadSize))
		{
			Serial.println("Ai o caralh ffsadasdsadsao");

			return false;
		}
#endif

		if (!LoLaDriver->PacketMap.RegisterMapping(&DefinitionPKERequest) ||
			!LoLaDriver->PacketMap.RegisterMapping(&DefinitionPKEResponse) ||
			!LoLaDriver->PacketMap.RegisterMapping(&DefinitionStartLink) ||
			!LoLaDriver->PacketMap.RegisterMapping(&DefinitionChallenge) ||
			!LoLaDriver->PacketMap.RegisterMapping(&DefinitionClockSync) ||
			!LoLaDriver->PacketMap.RegisterMapping(&DefinitionReport))
		{
			Serial.println("Ai o caralho");
			return false;
		}

		return LoLaPacketService::Setup();
	}

public:
	// Overload and do nothing, since we are the emiter.
	virtual void OnLinkStatusChanged(const bool linked) {}

protected:
	void ResetStateStartTime()
	{
		StateStartTime = millis();
	}

	uint32_t GetElapsedMillisSinceStateStart()
	{
		if (StateStartTime == 0)
		{
			return UINT32_MAX;
		}
		else
		{
			return millis() - StateStartTime;
		}
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link service"));
	}
#endif // DEBUG_LOLA
};
#endif