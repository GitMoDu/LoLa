// LoLaLinkDefinitions.h

#ifndef _LOLALINKDEFINITIONS_h
#define _LOLALINKDEFINITIONS_h

#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#include <LoLaCrypto\LoLaCryptoKeyExchanger.h>

#include <LoLaLinkInfo.h>
#include <LoLaDefinitions.h>
///





// Link sub-headers.

// ClockSync.

//#define LOLA_LINK_SUBHEADER_LINK_DISCOVERY					0x00
//#define LOLA_LINK_SUBHEADER_HOST_ID_BROADCAST				0x10

//Public Key Cryptography (PKC) headers.
//#define LOLA_LINK_SUBHEADER_REMOTE_KE_START_REQUEST			0x10
//#define LOLA_LINK_SUBHEADER_HOST_KE_RESPONSE				0x20

//Linking packets.
//#define LOLA_LINK_SUBHEADER_INFO_SYNC_REQUEST				0x40
//#define LOLA_LINK_SUBHEADER_INFO_SYNC_HOST					0x41
//#define LOLA_LINK_SUBHEADER_NTP_REPLY						0x42
//#define LOLA_LINK_SUBHEADER_UTC_REPLY						0x43
//
//#define LOLA_LINK_SUBHEADER_INFO_SYNC_REMOTE				0x50
//#define LOLA_LINK_SUBHEADER_NTP_REQUEST						0X51
//#define LOLA_LINK_SUBHEADER_UTC_REQUEST						0X52
//
//
//#define LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST				0x90
//#define LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY					0x91
//
//#define LOLA_LINK_SUBHEADER_LINK_REPORT						0x0A
//#define LOLA_LINK_SUBHEADER_LINK_REPORT_WITH_REPLY			0x0B


//Packet definition headers.
#define LOLA_LINK_HEADER_PKE_REQUEST						(PACKET_DEFINITION_LINK_START_HEADER)
#define LOLA_LINK_HEADER_PKE_RESPONSE						(LOLA_LINK_HEADER_PKE_REQUEST + 1)
#define LOLA_LINK_HEADER_LINK_START							(LOLA_LINK_HEADER_PKE_RESPONSE + 1)

#define LOLA_LINK_HEADER_MULTI								(LOLA_LINK_HEADER_LINK_START + 1)

#define LOLA_LINK_HEADER_REPORT								(LOLA_LINK_HEADER_MULTI + 1)
#define LOLA_LINK_HEADER_PING_WITH_ACK						(LOLA_LINK_HEADER_REPORT + 1)


class PingPacketDefinition : public PacketDefinition
{

public:
	PingPacketDefinition(IPacketListener* service)
		: PacketDefinition(service,
			LOLA_LINK_HEADER_PING_WITH_ACK,
			0,// Only payload is Id. 
			PacketDefinition::PACKET_DEFINITION_MASK_HAS_ACK)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Ping"));
	}
#endif
};

class LinkMultiPacketDefinition : public PacketDefinition
{
public:
	LinkMultiPacketDefinition(IPacketListener* service) :
		PacketDefinition(service,
			LOLA_LINK_HEADER_MULTI,
			sizeof(uint32_t)*2)  // 2 x 32 bit payload.
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-Linker"));
	}
#endif
};

class LinkStartWithAckPacketDefinition : public PacketDefinition
{
public:
	LinkStartWithAckPacketDefinition(IPacketListener* service) :
		PacketDefinition(service,
			LOLA_LINK_HEADER_LINK_START,
			0,
			PacketDefinition::PACKET_DEFINITION_MASK_HAS_ACK)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-ShortAck"));
	}
#endif
};

class LinkReportPacketDefinition : public PacketDefinition
{
public:
	LinkReportPacketDefinition(IPacketListener* service) :
		PacketDefinition(service,
			LOLA_LINK_HEADER_REPORT,
			2)// 1 byte for RSSI + 1 byte 
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
	LinkPKERequestPacketDefinition(IPacketListener* service) :
		PacketDefinition(service,
			LOLA_LINK_HEADER_PKE_REQUEST,
			sizeof(uint8_t) + sizeof(uint32_t) + LoLaCryptoKeyExchanger::KEY_MAX_SIZE)// Request Id + Protocol Id + Public Key.
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
	LinkPKEResponsePacketDefinition(IPacketListener* service) :
		PacketDefinition(service,
			LOLA_LINK_HEADER_PKE_RESPONSE,
			sizeof(uint8_t) + sizeof(uint32_t) + LoLaCryptoKeyExchanger::KEY_MAX_SIZE, // Request Id + Session Salt + Public Key.
			PacketDefinition::PACKET_DEFINITION_MASK_HAS_ACK)
	{}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-PKEResponse"));
	}
#endif
};
#endif