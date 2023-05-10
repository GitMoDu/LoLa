// PacketEventEnum.h

#ifndef _PACKET_EVENT_ENUM_h
#define _PACKET_EVENT_ENUM_h

enum PacketEventEnum
{
	Received,
	ReceivedAck,
	Sent,
	SentWithAck,
	SentAck,
	SendAckFailed,
	SendCollisionFailed,
	ReceiveRejectedDriver,
	ReceiveRejectedDropped,
	ReceiveRejectedInvalid,
	ReceiveRejectedOutOfSlot,
	ReceiveRejectedAck,
	ReceiveRejectedMac,
	ReceiveRejectedCounter
};
#endif