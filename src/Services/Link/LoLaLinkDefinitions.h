// LoLaLinkDefinitions.h

#ifndef _LOLALINKDEFINITIONS_h
#define _LOLALINKDEFINITIONS_h

#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#include <LoLaLinkInfo.h>
#include <LoLaCrypto\LoLaCryptoKeyExchange.h>

#define LOLA_LINK_PROTOCOL_VERSION							0 //N < 16


#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_PING					0 //Only payload is Id.
#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_REPORT				3
#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_SHORT				(1 + sizeof(uint32_t))  //1 byte Sub-header + 4 byte payload for uint32.
#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_SHORT_WITH_ACK		(sizeof(uint32_t))	//1 byte encoded Partner Id.
#define LOLA_LINK_SERVICE_PAYLOAD_SIZE_LONG					(1 + max(LOLA_LINK_CRYPTO_KEY_MAX_SIZE, LOLA_LINK_INFO_MAC_LENGTH))  //1 byte Sub-header + biggest payload size.		

#define LOLA_LINK_SERVICE_PACKET_MAX_SIZE					(LOLA_PACKET_MIN_SIZE_WITH_ID + LOLA_LINK_SERVICE_PAYLOAD_SIZE_LONG)

//Linked.
#define LOLA_LINK_SERVICE_LINKED_RESEND_PERIOD				(uint32_t)(15)
#define LOLA_LINK_SERVICE_LINKED_RESEND_SLOW_PERIOD			(uint32_t)(35)
#define LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT		(uint32_t)(1000)
#define LOLA_LINK_SERVICE_LINKED_PERIOD_INTERVENTION		(uint32_t)((LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT*6)/10)
#define LOLA_LINK_SERVICE_LINKED_MAX_PANIC					(uint32_t)((LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT*8)/10)
#define LOLA_LINK_SERVICE_LINKED_INFO_UPDATE_PERIOD			(uint32_t)((LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT*5)/10)
#define LOLA_LINK_SERVICE_LINKED_INFO_STALE_PERIOD			(uint32_t)(LOLA_LINK_SERVICE_LINKED_PERIOD_INTERVENTION)
#define LOLA_LINK_SERVICE_LINKED_POWER_UPDATE_PERIOD		(uint32_t)((LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT*3)/10)

//Not linked.
#define LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_LINKING_CANCEL	(uint32_t)(500) //Typical value is < 150 ms
#define LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKC_CANCEL		(uint32_t)(1000) //Typical value is < 200 ms
#define LOLA_LINK_SERVICE_UNLINK_HOST_MAX_BEFORE_SLEEP		(uint32_t)(10000)
#define LOLA_LINK_SERVICE_UNLINK_REMOTE_MAX_BEFORE_SLEEP	(uint32_t)(30000)
#define LOLA_LINK_SERVICE_UNLINK_HOST_SLEEP_PERIOD			(uint32_t)(UINT32_MAX)
#define LOLA_LINK_SERVICE_UNLINK_REMOTE_SLEEP_PERIOD		(uint32_t)(3000)
#define LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD				(uint32_t)(15)
#define LOLA_LINK_SERVICE_UNLINK_PING_RESEND_PERIOD			(uint32_t)(10)
#define LOLA_LINK_SERVICE_UNLINK_RESEND_LONG_PERIOD			(uint32_t)(30)
#define LOLA_LINK_SERVICE_UNLINK_BROADCAST_PERIOD			(uint32_t)(50)
#define LOLA_LINK_SERVICE_UNLINK_REMOTE_SEARCH_PERIOD		(uint32_t)(75)
#define LOLA_LINK_SERVICE_UNLINK_TRANSACTION_LIFETIME		(uint32_t)(100)
#define LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES		(uint8_t)(3)  //Max 25.
#define LOLA_LINK_SERVICE_UNLINK_MAX_LATENCY_SAMPLES		(uint8_t)(LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES+2)
#define LOLA_LINK_SERVICE_UNLINK_SESSION_LIFETIME			(uint32_t)(LOLA_LINK_SERVICE_UNLINK_HOST_MAX_BEFORE_SLEEP / 2)
#define LOLA_LINK_SERVICE_UNLINK_KEY_PAIR_LIFETIME			(uint32_t)(60000) //Key pairs last 60 seconds.

///Timings.
#define LOLA_LINK_SERVICE_CHECK_PERIOD						(uint32_t)(1)
#define LOLA_LINK_SERVICE_IDLE_PERIOD						(uint32_t)(LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT/10)
///


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


//Link Acked switch over status packets.
//#define LOLA_LINK_SUBHEADER_ACK_PROTOCOL_SWITCHOVER			0xFE

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
	LinkingDone = 4
};

class PingPacketDefinition : public PacketDefinition
{
public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID; }
	uint8_t GetHeader() { return LOLA_LINK_HEADER_PING_WITH_ACK; }
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_PING; }

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
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ID; }
	uint8_t GetHeader() { return LOLA_LINK_HEADER_REPORT; }
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_REPORT; }

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
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ID; }
	uint8_t GetHeader() { return LOLA_LINK_HEADER_SHORT; }
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_SHORT; }

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
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ID | PACKET_DEFINITION_MASK_HAS_ACK; }
	uint8_t GetHeader() { return LOLA_LINK_HEADER_SHORT_WITH_ACK; }
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_SHORT_WITH_ACK; }

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
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ID; }
	uint8_t GetHeader() { return LOLA_LINK_HEADER_LONG; }
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PAYLOAD_SIZE_LONG; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("L-Long\t"));
	}
#endif
};

#endif