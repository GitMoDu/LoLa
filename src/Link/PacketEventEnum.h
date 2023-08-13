// PacketEventEnum.h

#ifndef _PACKET_EVENT_ENUM_h
#define _PACKET_EVENT_ENUM_h

enum PacketEventEnum
{
	Received,
	Sent,
	SendCollisionFailed,
	ReceiveRejectedTransceiver,
	ReceiveRejectedDropped,
	ReceiveRejectedInvalid,
	ReceiveRejectedOutOfSlot,
	ReceiveRejectedMac,
	ReceiveRejectedCounter,
	ReceiveLossDetected
};
#endif