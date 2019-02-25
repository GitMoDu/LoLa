// LoLaLinkDefinitions.h

#ifndef _LOLALINKDEFINITIONS_h
#define _LOLALINKDEFINITIONS_h

#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#include <LoLaLinkInfo.h>

#define LOLA_LINK_SERVICE_PACKET_LINK_LONG_PAYLOAD_SIZE			(1 + max(LOLA_LINK_CRYPTO_KEY_LENGTH, LOLA_LINK_INFO_MAC_LENGTH))  //1 byte Sub-header + biggest payload size.
#define LOLA_LINK_SERVICE_PACKET_LINK_PAYLOAD_SIZE				(1 + sizeof(uint32_t))  //1 byte Sub-header + 4 byte payload for uint32.
#define LOLA_LINK_SERVICE_PACKET_LINK_WITH_ACK_PAYLOAD_SIZE		(1)  //1 byte Sub-header


///Timings.
#define LOLA_LINK_SERVICE_CHECK_PERIOD						(uint32_t)(2)
#define LOLA_LINK_SERVICE_IDLE_PERIOD						(uint32_t)(50)


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
#define LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_CANCEL			(uint32_t)(200) //Typical value is < 150 ms
#define LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_SLEEP			(uint32_t)(10000)
#define LOLA_LINK_SERVICE_UNLINK_HOST_SLEEP_PERIOD			(uint32_t)(UINT32_MAX)
#define LOLA_LINK_SERVICE_UNLINK_REMOTE_SLEEP_PERIOD		(uint32_t)(3000)
#define LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD				(uint32_t)(10)
#define LOLA_LINK_SERVICE_UNLINK_BROADCAST_PERIOD			(uint32_t)(50)
#define LOLA_LINK_SERVICE_UNLINK_REMOTE_SEARCH_PERIOD		(uint32_t)(75)
#define LOLA_LINK_SERVICE_UNLINK_TRANSACTION_LIFETIME		(uint32_t)(50)
#define LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES		(uint8_t)(3)  //Max 25. Values below 4 will not required additional packets.
#define LOLA_LINK_SERVICE_UNLINK_MAX_LATENCY_SAMPLES		(uint8_t)(LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES+2)
#define LOLA_LINK_SERVICE_UNLINK_SESSION_LIFETIME			(uint32_t)(4*LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_CANCEL)
///


//Link packets headers.
#define LOLA_LINK_SUBHEADER_LINK_DISCOVERY					0x00
#define LOLA_LINK_SUBHEADER_HOST_BROADCAST					0x01
#define LOLA_LINK_SUBHEADER_REMOTE_LINK_REQUEST				0x02
#define LOLA_LINK_SUBHEADER_HOST_LINK_ACCEPTED				0x03
#define LOLA_LINK_SUBHEADER_REMOTE_LINK_READY				0x04
#define LOLA_LINK_SUBHEADER_CHALLENGE_REQUEST				0x05
#define LOLA_LINK_SUBHEADER_CHALLENGE_REPLY					0x06
#define LOLA_LINK_SUBHEADER_NTP_REQUEST						0x07
#define LOLA_LINK_SUBHEADER_NTP_REPLY						0x08
#define LOLA_LINK_SUBHEADER_NTP_TUNE_REQUEST				0x09
#define LOLA_LINK_SUBHEADER_NTP_TUNE_REPLY					0x10
#define LOLA_LINK_SUBHEADER_INFO_SYNC_UPDATE				0x0A

#define LOLA_LINK_SUBHEADER_LINK_INFO_REPORT				0x0B

#define LOLA_LINK_SUBHEADER_PING							0x0F
#define LOLA_LINK_SUBHEADER_PONG							0x0E


//Link Acked switch over status packets.
#define LOLA_LINK_SUBHEADER_ACK_LINK_REQUEST_SWITCHOVER		0xF1
#define LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER				0xF2
#define LOLA_LINK_SUBHEADER_ACK_CHALLENGE_SWITCHOVER		0xF3
#define LOLA_LINK_SUBHEADER_ACK_INFO_SYNC_ADVANCE			0xF4
#define LOLA_LINK_SUBHEADER_ACK_PROTOCOL_SWITCHOVER			0xFF


enum LinkingStagesEnum : uint8_t
{
	ClockSyncStage = 0,
	ClockSyncSwitchOver = 1,
	ChallengeStage = 2,
	ChallengeSwitchOver = 3,
	InfoSyncStage = 4,
	LinkProtocolSwitchOver = 5,
	AllConnectingStagesDone = 6
};

class LinkPacketWithAckDefinition : public PacketDefinition
{
public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID; }
	uint8_t GetHeader() { return PACKET_DEFINITION_LINK_WITH_ACK_HEADER; }
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PACKET_LINK_WITH_ACK_PAYLOAD_SIZE; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("LinkAck"));
	}
#endif
};

class LinkPacketDefinition : public PacketDefinition
{
public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ID; }
	uint8_t GetHeader() { return PACKET_DEFINITION_LINK_HEADER; }
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PACKET_LINK_PAYLOAD_SIZE; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link"));
	}
#endif
};

class LinkPacketLongDefinition : public PacketDefinition
{
public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_BASIC; }
	uint8_t GetHeader() { return PACKET_DEFINITION_LINK_LONG_HEADER; }
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PACKET_LINK_LONG_PAYLOAD_SIZE; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("LinkLong"));
	}
#endif
};

#endif