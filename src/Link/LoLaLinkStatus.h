// LoLaLinkStatus.h

#ifndef _LOLA_LINK_STATUS_h
#define _LOLA_LINK_STATUS_h

#include <stdint.h>
#include <LoLaDefinitions.h>

struct LoLaLinkStatus
{
	struct QualityStruct
	{
		uint8_t RxRssi = 0;
		uint8_t TxRssi = 0;
		uint8_t RxDrop = 0;
		uint8_t TxDrop = 0;
		uint8_t ClockSync = 0;
		uint8_t Age = 0;
	} Quality;

	uint32_t DurationSeconds = 0;

	uint16_t RxCount = 0;
	uint16_t TxCount = 0;

	uint16_t RxDropCount = 0;
	uint16_t RxDropRate = 0;
	uint16_t TxDropRate = 0;

#if defined(DEBUG_LOLA) || defined(DEBUG_LOLA_LINK)
	void Log(Stream& stream)
	{
		PrintDuration(stream, DurationSeconds);
		stream.print(F("\tRSSI <- "));
		stream.println(Quality.RxRssi);
		stream.print(F("\tRSSI -> "));
		stream.println(Quality.TxRssi);

		stream.print(F("\tTx Count: "));
		stream.println(TxCount);
		stream.print(F("\tRx Count: "));
		stream.println(RxCount);
		stream.print(F("\tRx Lost: "));
		stream.println(RxDropCount);
		stream.print(F("\tQuality Rx: "));
		stream.println(Quality.RxDrop);
		stream.print(F("\tRx Drop Rate: "));
		stream.println(RxDropRate);
		stream.print(F("\tQuality Tx: "));
		stream.println(Quality.TxDrop);
		stream.print(F("\tTx Drop Rate: "));
		stream.println(TxDropRate);
		stream.print(F("\tRx Youth: "));
		stream.println(Quality.Age);
		stream.print(F("\tSync Clock: "));
		stream.println(Quality.ClockSync);

		stream.println();
	}

	void PrintDuration(Stream& stream, const uint32_t durationSeconds)
	{
		const uint32_t durationMinutes = durationSeconds / 60;
		const uint32_t durationHours = durationMinutes / 60;
		const uint32_t durationDays = durationHours / 24;
		const uint32_t durationYears = durationDays / 365;

		const uint32_t durationDaysRemainder = durationDays % 365;
		const uint32_t durationHoursRemainder = durationHours % 24;
		const uint32_t durationMinutesRemainder = durationMinutes % 60;
		const uint32_t durationSecondsRemainder = durationSeconds % 60;

		if (durationYears > 0)
		{
			stream.print(durationYears);
			stream.print(F(" years "));

			stream.print(durationDaysRemainder);
			if (durationDaysRemainder > 1) {

				stream.print(F(" days "));
			}
			else
			{
				stream.print(F(" day "));
			}
		}
		else if (durationDays > 0)
		{
			stream.print(durationDays);
			if (durationDays > 1) {

				stream.print(F(" days "));
			}
			else
			{
				stream.print(F(" day "));
			}
		}

		if (durationHoursRemainder < 10)
		{
			stream.print(0);
		}
		stream.print(durationHoursRemainder);
		stream.print(':');
		if (durationMinutesRemainder < 10)
		{
			stream.print(0);
		}
		stream.print(durationMinutesRemainder);
		stream.print(':');
		if (durationSecondsRemainder < 10)
		{
			stream.print(0);
		}
		stream.print(durationSecondsRemainder);
		stream.println();
	}
#endif
};

class LoLaLinkExtendedStatus : public LoLaLinkStatus
{
private:
	uint16_t LastTxCount = 0;
	uint16_t LastRxCount = 0;
	uint16_t LastRxDropCount = 0;

	uint32_t LoopsTxCount = 0;
	uint32_t LoopsRxCount = 0;
	uint32_t LoopsRxDropCount = 0;

public:
	const uint64_t GetLongTxCount()
	{
		return (((uint64_t)LoopsTxCount) * UINT16_MAX) + TxCount;
	}

	const uint64_t GetLongRxCount()
	{
		return (((uint64_t)LoopsRxCount) * UINT16_MAX) + RxCount;
	}

	const uint64_t GetLongRxDropCount()
	{
		return (((uint64_t)LoopsRxDropCount) * UINT16_MAX) + RxDropCount;
	}

public:
	void OnStart()
	{
		LoopsRxDropCount = 0;
		LoopsTxCount = 0;
		LoopsRxCount = 0;

		LastTxCount = TxCount;
		LastRxCount = RxCount;
		LastRxDropCount = RxDropCount;
	}

	void OnUpdate()
	{
		if (TxCount < LastTxCount)
		{
			LoopsTxCount++;
		}
		LastTxCount = TxCount;

		if (RxCount < LastRxCount)
		{
			LoopsRxCount++;
		}
		LastRxCount = RxCount;

		if (RxDropCount < LastRxDropCount)
		{
			LoopsRxDropCount++;
		}
		LastRxDropCount = RxDropCount;
	}

#if defined(DEBUG_LOLA) || defined(DEBUG_LOLA_LINK)
	void LogLong(Stream& stream)
	{
		PrintDuration(stream, DurationSeconds);
		stream.print(F("\tRSSI <- "));
		stream.println(Quality.RxRssi);
		stream.print(F("\tRSSI -> "));
		stream.println(Quality.TxRssi);

		stream.print(F("\tTx Count: "));
		stream.println((uint32_t)GetLongTxCount());
		stream.print(F("\tRx Count: "));
		stream.println((uint32_t)GetLongRxCount());
		stream.print(F("\tRx Lost: "));
		stream.println((uint32_t)GetLongRxDropCount());
		stream.print(F("\tQuality Rx: "));
		stream.println(Quality.RxDrop);
		stream.print(F("\tRx Drop Rate: "));
		stream.println(RxDropRate);
		stream.print(F("\tQuality Tx: "));
		stream.println(Quality.TxDrop);
		stream.print(F("\tTx Drop Rate: "));
		stream.println(TxDropRate);
		stream.print(F("\tRx Youth: "));
		stream.println(Quality.Age);
		stream.print(F("\tSync Clock: "));
		stream.println(Quality.ClockSync);

		stream.println();
	}
#endif
};
#endif