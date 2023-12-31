// nRF24Support.h

#ifndef _LOLA_NRF24_SUPPORT_h
#define _LOLA_NRF24_SUPPORT_h

#include <stdint.h>


namespace nRF24
{
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
	static const nRF24Timing NRF_TIMINGS[3] = { {220, 470, 60, 116}, {190, 310, 60, 116}, {400, 1390, 60, 120} };

	// On addresses with less than 5 bytes, LSByte will be repeated.
	static const uint8_t HighEntropyShortAddress[3] = { 0b00110011, 0b01010101, 0b01100101 };
}
#endif