// Si4463Events.h

#ifndef _SI4463_EVENTS_h
#define _SI4463_EVENTS_h

#include <stdint.h>

namespace Si4463Events
{
	enum class ErrorEnum : uint16_t
	{
		InterruptCollision = 1,
		TxTimeout = 1 << 1,
		TxCollision = 1 << 3,
		TxUnexpected = 1 << 4,
		RxOrphan = 1 << 5,
		RxTriggerCollision = 1 << 6,
		RxPacketCollision = 1 << 7,
		RxInvalidSize = 1 << 8
	};

	struct InterruptEventStruct
	{
		volatile uint32_t Timestamp = 0;
		volatile bool Pending = false;

		void Clear()
		{
			Pending = false;
		}
	};

	struct HopStruct
	{
		uint32_t StartTimestamp = 0;
		uint8_t Channel = 0;
		bool Requested = false;

		void Request()
		{
			Requested = true;
		}

		void Request(const uint8_t channel)
		{
			if (!Requested
				|| Channel != channel)
			{
				Channel = channel;
				Request();
			}
		}

		const bool Pending()
		{
			return Requested;
		}

		void Clear()
		{
			Requested = false;
		}
	};

	struct TxStruct
	{
		uint32_t StartTimestamp = 0;
		bool Pending = false;
		bool Collision = false;

		void Clear()
		{
			Pending = false;
			Collision = false;
		}
	};

	struct TxEventStruct
	{
		bool Pending = false;
		bool Collision = false;

		void Clear()
		{
			Pending = false;
			Collision = false;
		}
	};

	struct RxStruct
	{
		uint32_t StartTimestamp = 0;
		uint8_t Size = 0;
		uint8_t Rssi = 0;

		const bool Pending()
		{
			return Size > 0;
		}

		void Clear()
		{
			Size = 0;
		}
	};

	struct RxTriggerStruct
	{
		uint32_t StartTimestamp = 0;
		bool Pending = false;
		bool Collision = false;

		void Clear()
		{
			Pending = false;
			Collision = false;
		}
	};

	struct RxEventStruct
	{
		int16_t Rssi = 0;
		uint8_t Size = 0;
		bool Collision = false;

		const bool Pending()
		{
			return Size > 0;
		}

		void Clear()
		{
			Size = 0;
			Collision = false;
		}
	};
};
#endif