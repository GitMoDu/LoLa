// PacketEventEnum.h

#ifndef _PACKET_EVENT_ENUM_h
#define _PACKET_EVENT_ENUM_h

enum class PacketEventEnum : uint8_t
{
	SendCollisionFailed,
	ReceiveRejectedTransceiver,
	ReceiveRejectedMac,
	ReceiveRejectedHeader
};
#endif