// Sx1Support.h

#ifndef _LOLA_SX1_SUPPORT_h
#define _LOLA_SX1_SUPPORT_h

#include <stdint.h>

namespace Sx12
{

	static constexpr uint32_t BR_MAX = 0xFFFFFF;
	static constexpr uint8_t BR_MIN = 0x01;

	static constexpr uint32_t BIT_RATE_MAX = 300000;
	static constexpr uint16_t BIT_RATE_DEFAULT = 4800;

	static constexpr uint32_t OSCILATOR_FREQUENCY = 32000000;

	static constexpr uint32_t GetBrFromBitRate(const uint32_t bitRate)
	{
		return (32 * (OSCILATOR_FREQUENCY / bitRate)) & BR_MAX;
	}

	static constexpr uint8_t OSCILATOR_DEVIATION = 34;
	static constexpr uint8_t PRE_AMBLE_LENGTH = 10;
	static constexpr uint8_t PRE_AMBLE_DETECT = 6;
	static constexpr uint8_t SYNC_LENGTH = 1;
	static constexpr uint8_t SYNC_WORD = 0b10110101;

	enum class PulseShape : uint8_t
	{
		NoFilter = 0x00,
		GaussianBt03 = 0x08,
		GaussianBt05 = 0x09,
		GaussianBt07 = 0x0A,
		GaussianBt1 = 0x0B
	};

	enum class CrcType : uint8_t
	{
		NoCrc = 0x01,
		Crc1Byte = 0x00,
		Crc2Byte = 0x02,
		Crc1ByteInverted = 0x04,
		Crc2ByteInverted = 0x06
	};

	enum class PacketType : uint8_t
	{
		VariableLength = 0x00,
		FixedLength = 0x01
	};

	enum class AddrCompare : uint8_t
	{
		NoFiltering = 0x00,
		FilteringNodes = 0x01,
		FilteringNodesBroadcast = 0x02
	};

	enum class Whitening : uint8_t
	{
		NoWhitening = 0x00,
		WhiteningEnabled = 0x01
	};

	enum class RxBandWitdh : uint8_t
	{
		Bw4800 = 0x1F,
		Bw5800 = 0x17,
		Bw7300 = 0x0F,
		Bw9700 = 0x1E,
		Bw11700 = 0x16,
		Bw14600 = 0x0E,
		Bw19500 = 0x1D,
		Bw23400 = 0x15,
		Bw29300 = 0x0D,
		Bw39000 = 0x1C,
		Bw46900 = 0x14,
		Bw58600 = 0x0C,
		Bw78200 = 0x1B,
		Bw93800 = 0x13,
		Bw117300 = 0x0B,
		Bw156200 = 0x1A,
		Bw187200 = 0x12,
		Bw234200 = 0x0A,
		Bw312000 = 0x19,
		Bw373600 = 0x11,
		Bw467000 = 0x09
	};

	struct EventStruct
	{
		volatile uint32_t Timestamp = 0;
		volatile bool Pending = false;
		volatile bool DoubleEvent = false;

		void Clear()
		{
			Pending = false;
			DoubleEvent = false;
		}
	};

	struct PacketEventStruct
	{
		uint32_t Timestamp = 0;
		bool TxOk = false;
		bool TxFail = false;
		bool RxReady = false;

		const bool Pending()
		{
			return TxOk || TxFail || RxReady;
		}

		void Clear()
		{
			TxOk = false;
			TxFail = false;
			RxReady = false;
		}
	};
}
#endif