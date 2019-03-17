// LoLaDefinitions.h

#ifndef _LOLADEFINITIONS_h
#define _LOLADEFINITIONS_h

//#define LOLA_MOCK_RADIO
//#define LOLA_MOCK_PACKET_LOSS

#define LOLA_LINK_USE_LATENCY_COMPENSATION

#define LOLA_LINK_USE_ENCRYPTION
//#define DEBUG_LINK_ENCRYPTION

#define LOLA_LINK_USE_FREQUENCY_HOP
//#define DEBUG_LINK_FREQUENCY_HOP

#define LOLA_LINK_DEBUG_UPDATE_SECONDS						60

#define LOLA_LINK_PROTOCOL_VERSION							1 //N < 16


// 100 High latency, high bandwitdh.
// 10 Low latency, lower bandwidth.
#define ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS					10


// Packet collision avoidance.
#define LOLA_LINK_UNLINKED_BACK_OFF_DURATION_MILLIS			(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS/4)

#define LOLA_LINK_INFO_MAC_LENGTH							8 //Following MAC-64, because why not?

#define RADIO_POWER_BALANCER_RSSI_SAMPLE_COUNT				3


///Driver constants.
#define ILOLA_DEFAULT_MIN_RSSI								(int16_t(-100))


#define MOCK_PACKET_LOSS_SOFT								8
#define MOCK_PACKET_LOSS_HARD								12
#define MOCK_PACKET_LOSS_SMOKE_SIGNALS						35
#define MOCK_PACKET_LOSS_LINKING							MOCK_PACKET_LOSS_HARD
#define MOCK_PACKET_LOSS_LINKED								MOCK_PACKET_LOSS_SOFT

// How long to stay on a channel/token. TODO: Reduce when clocksync is better.
#define LOLA_LINK_SERVICE_LINKED_TIMED_HOP_PERIOD_MILLIS	(uint32_t)(1000) 

//Not linked.
#define LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES		(uint8_t)(3)
#define LOLA_LINK_SERVICE_UNLINK_MIN_CLOCK_SAMPLES			(uint8_t)(2)
#define LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_PKC_CANCEL		(uint32_t)(300) //Typical value is ~ 200 ms
#define LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_LINKING_CANCEL	(uint32_t)(200) //Typical value is ~ 100 ms
#define LOLA_LINK_SERVICE_UNLINK_HOST_MAX_BEFORE_SLEEP		(uint32_t)(10000)
#define LOLA_LINK_SERVICE_UNLINK_REMOTE_MAX_BEFORE_SLEEP	(uint32_t)(30000)
#define LOLA_LINK_SERVICE_UNLINK_HOST_SLEEP_PERIOD			(uint32_t)(UINT32_MAX)
#define LOLA_LINK_SERVICE_UNLINK_REMOTE_SLEEP_PERIOD		(uint32_t)(3000)
#define LOLA_LINK_SERVICE_UNLINK_RESEND_PERIOD				(uint32_t)(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS/2)
#define LOLA_LINK_SERVICE_UNLINK_PING_RESEND_PERIOD_MAX		(uint32_t)(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS/2)
#define LOLA_LINK_SERVICE_UNLINK_RESEND_LONG_PERIOD			(uint32_t)(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS*2)
#define LOLA_LINK_SERVICE_UNLINK_BROADCAST_PERIOD			(uint32_t)(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS*5)
#define LOLA_LINK_SERVICE_UNLINK_REMOTE_SEARCH_PERIOD		(uint32_t)(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS*7)
#define LOLA_LINK_SERVICE_UNLINK_TRANSACTION_LIFETIME		(uint32_t)(LOLA_LINK_SERVICE_UNLINK_MAX_BEFORE_LINKING_CANCEL)
#define LOLA_LINK_SERVICE_UNLINK_MAX_LATENCY_SAMPLES		(uint8_t)(LOLA_LINK_SERVICE_UNLINK_MIN_LATENCY_SAMPLES+2)
#define LOLA_LINK_SERVICE_UNLINK_SESSION_LIFETIME			(uint32_t)(LOLA_LINK_SERVICE_UNLINK_HOST_MAX_BEFORE_SLEEP/2)
#define LOLA_LINK_SERVICE_UNLINK_KEY_PAIR_LIFETIME			(uint32_t)(60000) //Key pairs last 60 seconds.

//Linked.
#define LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT		(uint32_t)(3000)
#define LOLA_LINK_SERVICE_LINKED_RESEND_PERIOD				(uint32_t)(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS)
#define LOLA_LINK_SERVICE_LINKED_RESEND_SLOW_PERIOD			(uint32_t)(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS*5)
#define LOLA_LINK_SERVICE_LINKED_PERIOD_INTERVENTION		(uint32_t)((LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT*6)/10)
#define LOLA_LINK_SERVICE_LINKED_MAX_PANIC					(uint32_t)((LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT*8)/10)
#define LOLA_LINK_SERVICE_LINKED_INFO_UPDATE_PERIOD			(uint32_t)((LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT*5)/10)
#define LOLA_LINK_SERVICE_LINKED_INFO_STALE_PERIOD			(uint32_t)(LOLA_LINK_SERVICE_LINKED_PERIOD_INTERVENTION)
#define LOLA_LINK_SERVICE_LINKED_POWER_UPDATE_PERIOD		(uint32_t)((LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT*3)/10)
#define LOLA_LINK_SERVICE_LINKED_CLOCK_TUNE_PERIOD			(uint32_t)(8000) //Agressive clock sync, until we upgrade to RTC.


///Timings.
#define LOLA_LINK_SEND_FAIL_RETRY_PERIOD					(uint32_t)(ILOLA_DEFAULT_DUPLEX_PERIOD_MILLIS*5)
#define LOLA_LINK_SERVICE_CHECK_PERIOD						(uint32_t)(1)
#define LOLA_LINK_SERVICE_IDLE_PERIOD						(uint32_t)(LOLA_LINK_SERVICE_LINKED_MAX_BEFORE_DISCONNECT/10)
///

///Specialized constants.
#define ILOLA_INVALID_RSSI									((int16_t)INT16_MIN)
#define ILOLA_INVALID_RSSI_NORMALIZED						((uint8_t)0)
#define ILOLA_INVALID_MILLIS								((uint32_t)UINT32_MAX)
#define ILOLA_INVALID_MICROS								ILOLA_INVALID_MILLIS
#define ILOLA_INVALID_LATENCY								((uint16_t)UINT16_MAX)
///


#endif