// LoLaLinkDefinitions.h

#ifndef _LOLALINKDEFINITIONS_h
#define _LOLALINKDEFINITIONS_h

#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#include <LoLaLinkInfo.h>
#include <LoLaDefinitions.h>
///

///Link packet sizes.
#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_PING					0 //Only payload is Id.
#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_REPORT				5
#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_SHORT				(1 + sizeof(uint32_t))  //1 byte Sub-header + 4 byte payload for uint32.
#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_SHORT_WITH_ACK		(sizeof(uint32_t))	//4 byte encoded Partner Id.
#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_LONG					(1 + max(LoLaCryptoKeyExchanger::KEY_MAX_SIZE, LOLA_LINK_INFO_MAC_LENGTH))  //1 byte Sub-header + biggest payload size.		

#define LOLA_LINK_SERVICE_PACKET_MAX_SIZE					(LOLA_PACKET_MIN_PACKET_SIZE + LOLA_LINK_SERVICE_PAYLOAD_SIZE_LONG)


///Link headers.
//Pre-Linking headers, with space for protocol versioning.
#define LOLA_LINK_SUBHEADER_LINK_DISCOVERY					0x00 + LOLA_LINK_PROTOCOL_VERSION
#define LOLA_LINK_SUBHEADER_HOST_ID_BROADCAST				0x10 + LOLA_LINK_PROTOCOL_VERSION

//Public Key Cryptography (PKC) headers.
#define LOLA_LINK_SUBHEADER_HOST_PUBLIC_KEY					0x20

#define LOLA_LINK_SUBHEADER_REMOTE_PKC_START_REQUEST		0x30
#define LOLA_LINK_SUBHEADER_REMOTE_PUBLIC_KEY				0x31

//Linking packets.
#define LOLA_LINK_SUBHEADER_INFO_SYNC_REQUEST				0x40
#define LOLA_LINK_SUBHEADER_INFO_SYNC_HOST					0x41
#define LOLA_LINK_SUBHEADER_NTP_REPLY						0x42

#define LOLA_LINK_SUBHEADER_INFO_SYNC_REMOTE				0x50
#define LOLA_LINK_SUBHEADER_NTP_REQUEST						0X51


#define LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST				0x90
#define LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY					0x91

#define LOLA_LINK_SUBHEADER_LINK_REPORT						0x0A
#define LOLA_LINK_SUBHEADER_LINK_REPORT_WITH_REPLY			0x0B


//Packet definition headers.
#define LOLA_LINK_HEADER_PING_WITH_ACK						(PACKET_DEFINITION_LINK_START_HEADER)
#define LOLA_LINK_HEADER_SHORT								(LOLA_LINK_HEADER_PING_WITH_ACK + 1)
#define LOLA_LINK_HEADER_SHORT_WITH_ACK						(LOLA_LINK_HEADER_SHORT + 1)
#define LOLA_LINK_HEADER_LONG								(LOLA_LINK_HEADER_SHORT_WITH_ACK + 1)
#define LOLA_LINK_HEADER_REPORT								(LOLA_LINK_HEADER_LONG + 1)


enum LinkingStagesEnum : uint8_t
{
	InfoSyncStage = 0,
	ClockSyncStage = 1,
	LinkProtocolSwitchOver = 2,
	LinkingDone = 3
};

class PingPacketDefinition : public PacketDefinition
{
public:
	const uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK; }
	const uint8_t GetHeader() { return LOLA_LINK_HEADER_PING_WITH_ACK; }
	const uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_PING; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Ping\t"));
	}
#endif
};

class LinkReportPacketDefinition : public PacketDefinition
{
public:
	const uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC; }
	const uint8_t GetHeader() { return LOLA_LINK_HEADER_REPORT; }
	const uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_REPORT; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-Report"));
	}
#endif
};

class LinkShortPacketDefinition : public PacketDefinition
{
public:
	const uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC; }
	const uint8_t GetHeader() { return LOLA_LINK_HEADER_SHORT; }
	const uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_SHORT; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-Short\t"));
	}
#endif
};

class LinkShortWithAckPacketDefinition : public PacketDefinition
{
public:
	const uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK; }
	const uint8_t GetHeader() { return LOLA_LINK_HEADER_SHORT_WITH_ACK; }
	const uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_SHORT_WITH_ACK; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-ShortAck"));
	}
#endif
};

class LinkLongPacketDefinition : public PacketDefinition
{
public:
	const uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC; }
	const uint8_t GetHeader() { return LOLA_LINK_HEADER_LONG; }
	const uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_LONG; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-Long\t"));
	}
#endif
};

#endif