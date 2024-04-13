// nRF24Support.h

#ifndef _LOLA_NRF24_SUPPORT_h
#define _LOLA_NRF24_SUPPORT_h

#include <stdint.h>

namespace nRF24Support
{
	static constexpr uint16_t TRANSCEIVER_ID = 0x3F24;

	// The SPI interface is designed to operate at a maximum of 10 MHz.
	// Most modules support 16-18 MHz.
#if defined(ARDUINO_ARCH_AVR)
	static constexpr uint32_t NRF24_SPI_SPEED = 8000000;
#elif defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
	static constexpr uint32_t NRF24_SPI_SPEED = 16000000;
#elif defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
	static constexpr uint32_t NRF24_SPI_SPEED = 16000000;
#else
	static constexpr uint32_t NRF24_SPI_SPEED = RF24_SPI_SPEED;
#endif


	struct nRF24Timing
	{
		uint16_t RxDelayMin;
		uint16_t RxDelayMax;
		uint16_t TxDelayMin;
		uint16_t TxDelayMax;
	};

	/// <summary>
	/// Approximate values measured on real hardware.
	/// Timings ordered by Baudarate enum value.
	/// [RF24_1MBPS|RF24_2MBPS|RF24_250KBPS] 
	/// </summary>
	static constexpr nRF24Timing NRF_TIMINGS[3] = { {.RxDelayMin = 220, .RxDelayMax = 470, .TxDelayMin = 60, .TxDelayMax = 116},
												{.RxDelayMin = 190, .RxDelayMax = 310, .TxDelayMin = 60, .TxDelayMax = 116},
												{.RxDelayMin = 400, .RxDelayMax = 1390, .TxDelayMin = 60, .TxDelayMax = 120} };

	// Fixed addressing identifies protocol.
	static constexpr uint8_t AddressSize = 3;

	// On addresses with less than 5 bytes, LSByte will be repeated.
	static const uint8_t HighEntropyShortAddress[AddressSize] = { 0b00110011, 0b01010101, 0b01100101 };

	// 126 RF channels, 1 MHz steps.
	static constexpr uint8_t ChannelCount = 126;

	// 130us according to datasheet.
	static constexpr uint16_t RxHopDelay = 130;

	// Only one pipe for protocol.
	static constexpr uint8_t AddressPipe = 0;

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