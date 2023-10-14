// PacketEventEnum.h

#ifndef _PACKET_EVENT_ENUM_h
#define _PACKET_EVENT_ENUM_h

enum PacketEventEnum
{
	SendCollisionFailed,
	ReceiveRejectedTransceiver,
	ReceiveRejectedDropped,
	ReceiveRejectedInvalid,
	ReceiveRejectedOutOfSlot,
	ReceiveRejectedMac,
	ReceiveRejectedHeader
};
#endif