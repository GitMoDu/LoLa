// IVirtualPacketDriver.h

#ifndef _I_VIRTUAL_PACKET_DRIVER_h
#define _I_VIRTUAL_PACKET_DRIVER_h

#include <stdint.h>


class IVirtualPacketDriver
{
public:
	template<const uint8_t channelCount,
		const uint32_t txBase,
		const uint32_t txByteNanos,
		const uint32_t rxBase,
		const uint32_t rxByteNanos,
		const uint32_t hopMicros>
	struct Configuration
	{
		static constexpr uint8_t ChannelCount = channelCount;
		static constexpr uint32_t TxBase = txBase;
		static constexpr uint32_t TxByteNanos = txByteNanos;
		static constexpr uint32_t RxBase = rxBase;
		static constexpr uint32_t RxByteNanos = rxByteNanos;
		static constexpr uint32_t HopMicros = hopMicros;
	};

public:
	virtual void SetPartner(IVirtualPacketDriver* partner) {}
	virtual void ReceivePacket(const uint8_t* data, const uint8_t size, const uint8_t channel) {}
};
#endif