// LoLaSi446xSupport.h

#ifndef _LOLA_SI446X_SUPPORT_h
#define _LOLA_SI446X_SUPPORT_h

#include <stdint.h>


struct LoLaSi446xSupport
{
	struct RxHopStruct
	{
	public:
		uint32_t Start = 0;
		uint8_t Channel = 0;

	private:
		bool PendingInternal = false;

	public:
		void Clear()
		{
			PendingInternal = false;
		}

		void SetPending(const uint32_t startTimestamp, const uint8_t channel)
		{
			Start = startTimestamp;
			Channel = channel;
			PendingInternal = true;
		}

		const bool Pending()
		{
			return PendingInternal;
		}
	};

	struct InterruptEventStruct
	{
		volatile uint32_t Timestamp = 0;
		volatile bool Pending = false;
		volatile bool Double = false;

		void Clear()
		{
			Pending = false;
			Double = false;
		}
	};

	struct RxEventStruct
	{
		uint32_t Start = 0;
		uint32_t Timestamp = 0;
		uint8_t Size = 0;
		uint8_t RssiLatch = 0;

		void SetPending(const uint32_t startTimestamp, const uint8_t size)
		{
			Start = startTimestamp;
			Size = size;
		}

		const bool Pending()
		{
			return Size > 0;
		}

		void Clear()
		{
			Size = 0;
		}
	};

	struct TxEventStruct
	{
		uint32_t Start = 0;
		bool Pending = false;

		void Clear()
		{
			Pending = false;
		}
	};
};
#endif