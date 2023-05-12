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
	template<const uint8_t MaxPacketSize>
	struct IoPacketStruct
	{
		uint8_t Buffer[MaxPacketSize]{};
		uint32_t Started = 0;
		uint8_t Size = 0;
		const bool HasPending()
		{
			return Size > 0;
		}

		void Clear()
		{
			Size = 0;
		}
	};

	template<const uint8_t MaxPacketSize>
	struct InPacketStruct : public IoPacketStruct<MaxPacketSize> {};

	template<const uint8_t MaxPacketSize>
	struct DelayPacketStruct : IoPacketStruct<MaxPacketSize>
	{
		uint8_t Channel = 0;
	};



	template<const uint8_t MaxPacketSize>
	struct OutPacketStruct : IoPacketStruct<MaxPacketSize>
	{
		bool Delivered = false;
	};

#if defined(DEBUG_LOLA)
	static void LogChannel(const uint8_t channel, const uint8_t channelCount, const uint8_t channelDivisor)
	{
		const uint_fast8_t channelScaled = channel % channelCount;

		for (uint_fast8_t i = 0; i < channelScaled; i++)
		{
			Serial.print('_');
		}


		Serial.print('/');
		Serial.print('\\');

		for (uint_fast8_t i = (channelScaled + 1); i < channelCount; i++)
		{
			Serial.print('_');
		}

		Serial.println();
	}
#endif

#if defined(PRINT_PACKETS)
	static void PrintPacket(uint8_t* data, const uint8_t size)
	{
		Serial.println();
		Serial.print('|');
		Serial.print('|');

		uint32_t mac = data[0];
		mac += data[1] << 8;
		mac += (uint32_t)data[2] << 16;
		mac += (uint32_t)data[3] << 24;
		Serial.print(mac);
		Serial.print('|');

		Serial.print(data[4]);
		Serial.print('|');

		for (uint8_t i = 5; i < size; i++)
		{
			Serial.print('0');
			Serial.print('x');
			if (data[i] < 0x10)
			{
				Serial.print(0);
			}
			Serial.print(data[i], HEX);
			Serial.print('|');
		}
		Serial.println('|');
}
#endif

public:
	virtual void SetPartner(IVirtualPacketDriver* partner) {}
	virtual void ReceivePacket(const uint8_t* data, const uint8_t size, const uint8_t channel) {}
};
#endif