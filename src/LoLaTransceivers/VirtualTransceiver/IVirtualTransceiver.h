// IVirtualTransceiver.h

#ifndef _I_VIRTUAL_TRANSCEIVER_h
#define _I_VIRTUAL_TRANSCEIVER_h

#include <stdint.h>

class IVirtualTransceiver
{
public:
	template<const uint8_t channelCount,
		const uint32_t txBaseMicros,
		const uint32_t txByteNanos,
		const uint32_t airBaseMicros,
		const uint32_t airByteNanos,
		const uint32_t hopMicros>
	struct Configuration
	{
		static constexpr uint8_t ChannelCount = channelCount;
		static constexpr uint32_t TxBaseMicros = txBaseMicros;
		static constexpr uint32_t TxByteNanos = txByteNanos;
		static constexpr uint32_t AirBaseMicros = airBaseMicros;
		static constexpr uint32_t AirByteNanos = airByteNanos;
		static constexpr uint32_t HopMicros = hopMicros;
	};

public:
	virtual const uint8_t GetChannelCount() { return 0; }
	virtual const uint8_t GetCurrentChannel() { return 0; }

public:
	virtual void SetPartner(IVirtualTransceiver* partner) {}
	virtual void ReceivePacket(const uint8_t* data, const uint32_t txTimestamp, const uint8_t size, const uint8_t channel) { }
};
#endif