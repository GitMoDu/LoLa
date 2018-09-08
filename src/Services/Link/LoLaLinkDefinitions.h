// LoLaLinkDefinitions.h

#ifndef _LOLALINKDEFINITIONS_h
#define _LOLALINKDEFINITIONS_h

#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#define LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE	5  //1 byte Sub-header + 4 byte Pseudo-MAC or other uint32_t

#define LOLA_LINK_SERVICE_POLL_PERIOD_MILLIS				(uint32_t)500
#define LOLA_LINK_SERVICE_BROADCAST_PERIOD					(uint32_t)750
#define LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP			(uint32_t)30000
#define LOLA_LINK_SERVICE_SLEEP_PERIOD						(uint32_t)60000
#define LOLA_LINK_SERVICE_MAX_SYNC_ELAPSED					(uint32_t)60000 //Synced clock invalidates after 1 minute without sync.
#define LOLA_LINK_SERVICE_MIN_ELAPSED_RESYNC				(uint32_t)10000
#define LOLA_LINK_SERVICE_MIN_ELAPSED_RESYNC_REQUEST		(uint32_t)1000

#define LOLA_LINK_SERVICE_LINK_RESEND_PERIOD				(uint32_t)200
#define LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD				(uint32_t)25
#define LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD			(uint32_t)500
#define LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD					(uint32_t)(LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD*2)
#define LOLA_LINK_SERVICE_PERIOD_INTERVENTION				(uint32_t)(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*2)
#define LOLA_LINK_SERVICE_PANIC								(uint32_t)(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*3)
#define LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT				(uint32_t)(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*4)
#define LOLA_LINK_SERVICE_MAX_BEFORE_CONNECTING_CANCEL		(uint32_t)(LOLA_LINK_SERVICE_BROADCAST_PERIOD*6)
#define LOLA_LINK_SERVICE_CONNECTING_CANCEL_SLEEP_DURATION	(uint32_t)LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD

//#define LOLA_LINK_SERVICE_SLOW_CHECK_PERIOD					LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD
#define LOLA_LINK_SERVICE_FAST_CHECK_PERIOD					(uint32_t)50

#define LOLA_LINK_SERVICE_SUBHEADER_HELLO					0x00
#define LOLA_LINK_SERVICE_SUBHEADER_HOST_BROADCAST			0x01
#define LOLA_LINK_SERVICE_SUBHEADER_REMOTE_LINK_REQUEST		0x02
//#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_REQUEST		0x03
//#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_REPLY		0x04
#define LOLA_LINK_SERVICE_SUBHEADER_NTP						0X05
#define LOLA_LINK_SERVICE_SUBHEADER_NTP_REPLY				0X06

#define LOLA_LINK_SERVICE_SUBHEADER_LINK_REQUEST_ACCEPTED	0x01
//#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED	0x02
#define LOLA_LINK_SERVICE_SUBHEADER_NTP_ACCEPTED			0x03


//TODO: Future.
//#define LOLA_CONNECTION_SERVICE_SUBHEADER_LINK_INFO		0X05

#define LOLA_LINK_SERVICE_CLOCK_SYNC_LOOP_PERIOD			(uint32_t)1000	


#define LOLA_LINK_SERVICE_INVALID_SESSION					0

//TODO: Replace with one time random number.
#define LOLA_LINK_HOST_PMAC									0x0E0F
#define LOLA_LINK_REMOTE_PMAC								0x1A1B
#define LOLA_LINK_SERVICE_INVALID_PMAC						UINT32_MAX


//class LinkShortPacketDefinition : public PacketDefinition
//{
//public:
//	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ID; }
//	uint8_t GetHeader() { return PACKET_DEFINITION_LINK_SHORT_HEADER; }
//	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE; }
//
//#ifdef DEBUG_LOLA
//	void PrintName(Stream* serial)
//	{
//		serial->print(F("LinkShort"));
//	}
//#endif
//};

enum ConnectingStagesEnum : uint8_t
{
	ChallengeStage = 0,
	ChallengeSwitchOver = 1,
	ClockSyncStage = 2,
	ClockSyncSwitchOver = 3,
	LinkProtocolStage = 4,
	LinkProtocolSwitchOver = 5,
	ConnectingStagesEnumSize = 6
};

class LinkPacketWithAckDefinition : public PacketDefinition
{
public:
	uint8_t GetConfiguration() { return PACKET_DEFINITION_MASK_HAS_ACK | PACKET_DEFINITION_MASK_HAS_ID; }
	uint8_t GetHeader() { return PACKET_DEFINITION_LINK_WITH_ACK_HEADER; }
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE; }

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
	uint8_t GetPayloadSize() { return LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE; }

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Link"));
	}
#endif
};
#endif