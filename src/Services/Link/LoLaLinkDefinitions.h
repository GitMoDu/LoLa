// LoLaLinkDefinitions.h

#ifndef _LOLALINKDEFINITIONS_h
#define _LOLALINKDEFINITIONS_h

#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#define LOLA_LINK_SERVICE_PACKET_LINK_PAYLOAD_SIZE			5  //1 byte Sub-header + 4 byte Pseudo-MAC or other uint32_t
#define LOLA_LINK_SERVICE_PACKET_LINK_WITH_ACK_PAYLOAD_SIZE	1  //1 byte Sub-header

#define LOLA_LINK_SERVICE_POLL_PERIOD_MILLIS				(uint32_t)100
#define LOLA_LINK_SERVICE_FAST_CHECK_PERIOD					(uint32_t)25
#define LOLA_LINK_SERVICE_LINK_CHECK_PERIOD					(uint32_t)5
#define LOLA_LINK_SERVICE_SLEEP_PERIOD						(uint32_t)UINT32_MAX

//Connected.
#define LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP			(uint32_t)30000
#define LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD					(uint32_t)1000
#define LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD			(uint32_t)(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD/2)
#define LOLA_LINK_SERVICE_LINK_RESEND_PERIOD				(uint32_t)(LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD/10)
#define LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT				(uint32_t)(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*3)


//Not connected.
#define LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD				(uint32_t)(LOLA_LINK_SERVICE_LINK_RESEND_PERIOD/5)
#define LOLA_LINK_SERVICE_BROADCAST_PERIOD					LOLA_LINK_SERVICE_LINK_RESEND_PERIOD

#define LOLA_LINK_SERVICE_CONNECTING_CANCEL_SLEEP_DURATION	LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD

#define LOLA_LINK_SERVICE_MAX_BEFORE_CONNECTING_CANCEL		(uint32_t)(300) //Typical value is < 200 ms
#define LOLA_LINK_SERVICE_PERIOD_INTERVENTION				(uint32_t)(LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT/2)
#define LOLA_LINK_SERVICE_PANIC								(uint32_t)((LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT*3)/4)

//Clock sync keeping.
//#define LOLA_LINK_SERVICE_MAX_SYNC_ELAPSED				(uint32_t)60000 //Synced clock invalidates after 1 minute without sync.
//#define LOLA_LINK_SERVICE_MIN_ELAPSED_RESYNC				(uint32_t)10000
//#define LOLA_LINK_SERVICE_MIN_ELAPSED_RESYNC_REQUEST		(uint32_t)1000


//Link packets headers.
#define LOLA_LINK_SUBHEADER_HELLO							0x00
#define LOLA_LINK_SUBHEADER_HOST_BROADCAST					0x01
#define LOLA_LINK_SUBHEADER_REMOTE_LINK_REQUEST				0x02
#define LOLA_LINK_SUBHEADER_HOST_LINK_ACCEPTED				0x03
#define LOLA_LINK_SUBHEADER_REMOTE_LINK_READY				0x04
#define LOLA_LINK_SUBHEADER_CHALLENGE_REQUEST				0x05
#define LOLA_LINK_SUBHEADER_CHALLENGE_REPLY					0x06
#define LOLA_LINK_SUBHEADER_NTP_REQUEST						0X07
#define LOLA_LINK_SUBHEADER_NTP_REPLY						0X08

#define LOLA_LINK_SUBHEADER_PING							0x09
#define LOLA_LINK_SUBHEADER_PONG							0x10


//Link Acked switch over status packets.
#define LOLA_LINK_SUBHEADER_ACK_LINK_REQUEST_SWITCHOVER		0x11
#define LOLA_LINK_SUBHEADER_ACK_NTP_SWITCHOVER				0x12
#define LOLA_LINK_SUBHEADER_ACK_CHALLENGE_SWITCHOVER		0x13
#define LOLA_LINK_SUBHEADER_ACK_PROTOCOL_SWITCHOVER			0x14


//TODO: Future.
//#define LOLA_LINK_SUBHEADER_LINK_REPORT						0X

#define LOLA_LINK_SERVICE_INVALID_SESSION					0


enum ConnectingStagesEnum : uint8_t
{
	ClockSyncStage = 0,
	ClockSyncSwitchOver = 1,
	ChallengeStage = 2,
	ChallengeSwitchOver = 3,
	LinkProtocolSwitchOver = 4,
	AllConnectingStagesDone = 5
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
#endif