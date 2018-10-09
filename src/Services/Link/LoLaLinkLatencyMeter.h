// LoLaLinkLatencyMeter.h

#ifndef _LOLA_LINK_LATENCY_METER_h
#define _LOLA_LINK_LATENCY_METER_h

#define LOLA_LATENCY_METER_DATA_POINT_STACK_SIZE		5
#define LOLA_LATENCYMETER_PING_TIMEOUT_MILLIS			(uint32_t)50
#define LOLA_LATENCY_SERVICE_PING_TIMEOUT_MICROS		((uint32_t)LOLA_LATENCY_SERVICE_PING_TIMEOUT_MILLIS*(uint32_t)1000)

#include <RingBufCPP.h>

class LoLaLinkLatencyMeter
{
private:
	uint32_t LastSentWith = ILOLA_INVALID_MICROS;
	uint8_t LastSentPacketId = 0;

	uint32_t Grunt;
	uint32_t DurationSum;
	RingBufCPP<uint16_t, LOLA_LATENCY_PING_DATA_POINT_STACK_SIZE> DurationStack;

public:
	LoLaLinkLatencyMeter()
	{
		Reset();
	}

	void Reset()
	{
		LastSentWith = ILOLA_INVALID_MICROS;
		LastSentPacketId = 0;
		while (!DurationStack.IsEmpty())
		{
			DurationStack.pull();
		}
	}

	uint8_t GetSampleCount()
	{
		return DurationStack.numElements();
	}

	uint16_t GetAverageLatency()
	{
		if (DurationStack.isEmpty())
		{
			return ILOLA_INVALID_LATENCY;//No data.
		}
		else
		{
			DurationSum = 0;
			for (uint8_t i = 0; i < DurationStack.numElements(); i++)
			{
				DurationSum += *DurationStack.peek(i);
			}

			//Value is always smaller than uint16, because samples with higher value are filtered out on acquisition.
			return (uint16_t)DurationSum / DurationStack.numElements();;
		}
	}

	void OnAckPacketSent(const uint8_t packetId)
	{
		LastSentWith = micros();
		LastSentPacketId = packetId;
	}

	void OnAckReceived(const uint8_t packetId)
	{
		if (LastSentWith != ILOLA_INVALID_MICROS &&
			LastSentPacketId == packetId)
		{
			Grunt = micros() - LastSentWith;

			if (Grunt < LOLA_LATENCY_SERVICE_PING_TIMEOUT_MICROS)
			{
				DurationStack.push((uint16t_t)Grunt);
			}
			else
			{
				//Outlier value discarded.
			}
		}
	}
};
#endif