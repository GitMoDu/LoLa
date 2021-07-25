// LoLaLinkDefinitions.h

#ifndef _LOLALINKDEFINITIONS_h
#define _LOLALINKDEFINITIONS_h

#include <LoLaDefinitions.h>
#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#include <LoLaCrypto\LoLaCryptoKeyExchanger.h>

#include <Link\LoLaLinkInfo.h>


//Packet definition headers.
struct LinkPackets
{
	static const uint8_t PKE_REQUEST = PacketDefinition::PACKET_DEFINITION_LINK_START_HEADER;
	static const uint8_t PKE_RESPONSE = PKE_REQUEST + 1;
	static const uint8_t LINK_START = PKE_RESPONSE + 1;
	static const uint8_t CHALLENGE = LINK_START + 1;
	static const uint8_t CLOCK_SYNC = CHALLENGE + 1;
	static const uint8_t REPORT = CLOCK_SYNC + 1;


	static const uint8_t MAX_PAYLOAD_SIZE = sizeof(uint32_t) + LoLaCryptoKeyExchanger::KEY_MAX_SIZE;
};

class LinkChallengePacketDefinition : public PacketDefinition
{
public:
	LinkChallengePacketDefinition(ILoLaService* service) :
		PacketDefinition(service,
			LinkPackets::CHALLENGE,
			sizeof(uint32_t))  // Signed hash of the session Id.
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-Challenge"));
	}
#endif
};

class LinkClockSyncPacketDefinition : public PacketDefinition
{
public:
	LinkClockSyncPacketDefinition(ILoLaService* service) :
		PacketDefinition(service,
			LinkPackets::CLOCK_SYNC,
			1 + sizeof(uint32_t))  // 1 byte + 32 bit payload.
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-Clock"));
	}
#endif
};

class LinkStartWithAckPacketDefinition : public PacketDefinition
{
public:
	LinkStartWithAckPacketDefinition(ILoLaService* service) :
		PacketDefinition(service,
			LinkPackets::LINK_START,
			0, // No payload.
			true)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-Start"));
	}
#endif
};

class LinkReportPacketDefinition : public PacketDefinition
{
public:
	LinkReportPacketDefinition(ILoLaService* service) :
		PacketDefinition(service,
			LinkPackets::REPORT,
			1 + 1)// 1 byte for ReplyRequest, 1 byte for Normalized RSSI.
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-Report"));
	}
#endif
};

class LinkPKERequestPacketDefinition : public PacketDefinition
{
public:
	LinkPKERequestPacketDefinition(ILoLaService* service) :
		PacketDefinition(service,
			LinkPackets::PKE_REQUEST,
			sizeof(uint32_t) + LoLaCryptoKeyExchanger::KEY_MAX_SIZE,	// Protocol Id + Public Key.
			true)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-PKERequest"));
	}
#endif
};

class LinkPKEResponsePacketDefinition : public PacketDefinition
{
public:
	LinkPKEResponsePacketDefinition(ILoLaService* service) :
		PacketDefinition(service,
			LinkPackets::PKE_RESPONSE,
			sizeof(uint32_t) + LoLaCryptoKeyExchanger::KEY_MAX_SIZE,	// Session Id + Public Key.
			true)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-PKEResponse"));
	}
#endif
};
#endif