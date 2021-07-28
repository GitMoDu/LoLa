// RollingCounters.h

#ifndef _ROLLING_COUNTERS_h
#define _ROLLING_COUNTERS_h

#include <stdint.h>

struct RollingCounters
{
	uint8_t Local = 0;
	uint8_t LastReceivedCounter = 0;
	uint8_t LastReceivedDelta = 0;
	uint8_t CounterSkipped = 0;

	bool LastReceivedPresent = false;


	const uint8_t GetLocalNext()
	{
		Local = (Local + 1) % UINT8_MAX;

		return Local;
	}

	const bool PushReceivedCounter(const uint8_t receivedPacketCounter)
	{
		// Packet counter can rollover and we wouldn't notice.
		CounterSkipped = receivedPacketCounter - LastReceivedCounter;

		if (CounterSkipped != 0)
		{
			LastReceivedCounter = receivedPacketCounter;

			return true;
		}
		else
		{
			return false;
		}
	}

	void SetReceivedCounter(const uint8_t receivedPacketCounter)
	{
		LastReceivedCounter = receivedPacketCounter;
		LastReceivedPresent = true;
		CounterSkipped = 0;
	}

	void Reset()
	{
		CounterSkipped = 0;
		Local = 0;
		LastReceivedPresent = false;
	}

	const uint8_t GetCounterLost()
	{
		if (CounterSkipped > 0)
		{
			return CounterSkipped - 1;
		}
		else
		{
			return 0;
		}
	}
};
#endif