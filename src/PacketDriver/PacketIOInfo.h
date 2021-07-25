// PacketIOInfo.h

#ifndef _PACKET_IO_INFO_h
#define _PACKET_IO_INFO_h


#include <PacketDriver\LoLaPacketDriverSettings.h>

class PacketIOInfo
{
private:
	class InputInfoType
	{
	public:
		volatile uint32_t Micros = 0;
		volatile int16_t RSSI = ILOLA_INVALID_RSSI;

		void Clear()
		{
			Micros = 0;
			RSSI = ILOLA_INVALID_RSSI;
		}
	};

	class InputInfoCounterType : public InputInfoType
	{
	public:
		volatile uint8_t LastCounter = 0;

		void Clear()
		{
			Micros = 0;
			RSSI = ILOLA_INVALID_RSSI;
			LastCounter = 0;
		}
	};

	class OutputInfoType
	{
	public:
		volatile uint32_t Millis = 0;

		void Clear()
		{
			Millis = 0;
		}

		const uint32_t GetElapsedMillis()
		{
			return millis() - Millis;
		}
	};

	struct RollingCountersStruct
	{
		uint8_t Local = 0;
		uint8_t LastReceivedCounter = 0;
		uint8_t LastReceivedDelta = 0;
		bool LastReceivedPresent = false;

		uint8_t CounterSkipped = 0;

		uint8_t GetLocalNext()
		{
			Local = (Local + 1) % UINT8_MAX;

			return Local;
		}

		bool PushReceivedCounter(const uint8_t receivedPacketCounter)
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
			Local = random(INT16_MAX) % UINT8_MAX;
			LastReceivedPresent = false;
		}

		uint8_t GetCounterLost()
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

public:
	// Incoming packet.
	InputInfoType LastReceived;
	InputInfoType LastValidReceived;


	// Outgoing packet.
	OutputInfoType LastValidSent;

	// Estimated transmission time in microseconds.
	// For use of estimated latency features
	const uint8_t ETTMillis;



#ifdef DEBUG_LOLA
	uint32_t TransmitedCount = 0;
	uint32_t TransmitFailuresCount = 0;
	uint32_t ReceivedCount = 0;
	uint32_t StateCollisionCount = 0;
	uint32_t TimingCollisionCount = 0;
	uint32_t RejectedCount = 0;
	uint32_t CounterMismatchCount = 0;
	uint32_t UnknownCount = 0;
#endif

private:
	LoLaPacketDriverSettings* Settings;

	// Rolling Packet Counters
	RollingCountersStruct RollingCounters;

public:
	PacketIOInfo(LoLaPacketDriverSettings* settings)
		: ETTMillis(settings->ETTM / IClock::ONE_MILLI_MICROS)
		, Settings(settings)
	{}

	void Reset()
	{
		LastReceived.Clear();
		LastValidReceived.Clear();
		LastValidSent.Clear();
		RollingCounters.Reset();
	}

	uint8_t GetCounterLost()
	{
		return  RollingCounters.GetCounterLost();
	}

	bool ValidateNextReceivedCounter(const uint8_t receivedPacketCounter)
	{
		if (RollingCounters.LastReceivedPresent)
		{
			// Max packet skip is INT8_MAX.
			if (RollingCounters.PushReceivedCounter(receivedPacketCounter))
			{
#ifdef DEBUG_LOLA
				CounterMismatchCount++;
#endif
				return false;
			}
		}
		else
		{
			RollingCounters.SetReceivedCounter(receivedPacketCounter);
		}

		return true;

	}

	uint8_t GetLastSentPacketId()
	{
		return RollingCounters.Local;
	}

	uint8_t GetNextPacketId()
	{
		return RollingCounters.GetLocalNext();
	}

	void PromoteLastReceived()
	{
		LastValidReceived.Micros = LastReceived.Micros;
		LastValidReceived.RSSI = LastReceived.RSSI;
		LastReceived.Clear();
#ifdef DEBUG_LOLA
		ReceivedCount++;
#endif
	}

	const uint32_t GetETTMMicros()
	{
		return Settings->ETTM;
	}

	int16_t GetLastRSSI()
	{
		return LastReceived.RSSI;
	}

	int16_t GetLastValidRSSI()
	{
		return LastValidReceived.RSSI;
	}

	const uint8_t GetRSSINormalized()
	{
		return (uint8_t)map(constrain(GetLastValidRSSI(), Settings->RSSIMin, Settings->RSSIMax),
			Settings->RSSIMin, Settings->RSSIMax,
			0, UINT8_MAX);
	}

	const uint32_t GetElapsedMillisLastValidReceived()
	{
		if (LastValidReceived.Micros == 0)
		{
			return UINT32_MAX;
		}
		else
		{
			return (micros() - LastValidReceived.Micros) / 1000;
		}
	}

	const  uint32_t GetElapsedMillisLastReceived()
	{
		if (LastReceived.Micros == 0)
		{
			return UINT32_MAX;
		}
		else
		{
			return (micros() - LastReceived.Micros) / 1000;
		}
	}

	const uint32_t GetElapsedMillisLastValidSent()
	{
		return LastValidSent.GetElapsedMillis();
	}

	uint32_t GetLastActivityElapsedMillis()
	{
		if (GetElapsedMillisLastValidSent() < GetElapsedMillisLastValidReceived())
		{
			return GetElapsedMillisLastValidSent();
		}
		else
		{
			return GetElapsedMillisLastValidReceived();
		}
	}

public:
#ifdef LOLA_MOCK_PACKET_LOSS
	bool GetLossChance()
	{
		if (LinkActive)
		{
			return random(100) + 1 >= MOCK_PACKET_LOSS_LINKED;
		}
		else
		{
			return random(100) + 1 >= MOCK_PACKET_LOSS_LINKING;
		}
	}
#endif
};
#endif