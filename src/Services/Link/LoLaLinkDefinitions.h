// LoLaLinkDefinitions.h

#ifndef _LOLALINKDEFINITIONS_h
#define _LOLALINKDEFINITIONS_h

#include <Packet\LoLaPacket.h>
#include <Packet\LoLaPacketMap.h>

#define LOLA_LINK_SERVICE_PACKET_DEFINITION_PAYLOAD_SIZE	5  //1 byte Sub-header + 4 byte pseudo-MAC or other uint32_t + 1 byte for RSSI indicator
#define LOLA_LINK_SERVICE_SHORT_DEFINITION_PAYLOAD_SIZE		2  //1 byte Sub-header + 1 byte PAYLOAD

#define LOLA_LINK_SERVICE_POLL_PERIOD_MILLIS				500
#define LOLA_LINK_SERVICE_BROADCAST_PERIOD					1000
#define LOLA_LINK_SERVICE_MAX_ELAPSED_BEFORE_SLEEP			30000
#define LOLA_LINK_SERVICE_SLEEP_PERIOD						60000
#define LOLA_LINK_SERVICE_MAX_SYNC_ELAPSED					60000 //Synced clock invalidates after 1 minute without sync.
#define LOLA_LINK_SERVICE_MIN_ELAPSED_RESYNC				10000
#define LOLA_LINK_SERVICE_MIN_ELAPSED_RESYNC_REQUEST		1000

#define LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD			500
#define LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD					(LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD*2)
#define LOLA_LINK_SERVICE_PERIOD_INTERVENTION				(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*2)
#define LOLA_LINK_SERVICE_PANIC								(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*3)
#define LOLA_LINK_SERVICE_MAX_BEFORE_DISCONNECT				(LOLA_LINK_SERVICE_KEEP_ALIVE_PERIOD*4)
#define LOLA_LINK_SERVICE_MAX_BEFORE_CONNECTING_CANCEL		(LOLA_LINK_SERVICE_BROADCAST_PERIOD*2)
#define LOLA_LINK_SERVICE_CONNECTING_MIN_DURATION			50

#define LOLA_LINK_SERVICE_SLOW_CHECK_PERIOD					LOLA_LINK_SERVICE_KEEP_ALIVE_SEND_PERIOD
#define LOLA_LINK_SERVICE_FAST_CHECK_PERIOD					50

#define LOLA_LINK_SERVICE_SUBHEADER_HELLO					0x00
#define LOLA_LINK_SERVICE_SUBHEADER_HOST_BROADCAST			0x01
#define LOLA_LINK_SERVICE_SUBHEADER_REMOTE_LINK_REQUEST		0x02

#define LOLA_LINK_SERVICE_SUBHEADER_LINK_REQUEST_ACCEPTED	0x01
//#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_REPLY			0x02
//#define LOLA_LINK_SERVICE_SUBHEADER_CHALLENGE_ACCEPTED		0x03
//#define LOLA_LINK_SERVICE_SUBHEADER_NTP						0X04
//#define LOLA_LINK_SERVICE_SUBHEADER_NTP_REPLY				0X05


//#define LOLA_LINK_SERVICE_SHORT_SUBHEADER_NTP_REPLY			0X02

//TODO: Future.
//#define LOLA_CONNECTION_SERVICE_SUBHEADER_LINK_INFO		0X05

#define LOLA_LINK_SERVICE_CLOCK_SYNC_LOOP_PERIOD			1000	


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
	ChallengeStage = 1,
	ClockSyncStage = 2,
	LinkProtocolStage = 3
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