// LoLaLinkStatus.h

#ifndef _LOLA_LINK_STATUS_h
#define _LOLA_LINK_STATUS_h

struct LoLaLinkStatus
{
	struct QualityStruct
	{
		uint8_t RxRssi;
		uint8_t TxRssi;
		uint8_t RxDrop;
		uint8_t TxDrop;
		uint8_t ClockSync;
		uint8_t Age;
	} Quality;

	uint32_t DurationSeconds;

	uint16_t RxDropRate;
	uint16_t TxDropRate;


#if defined(DEBUG_LOLA)
	void Log(Stream& stream)
	{
		PrintDuration(stream, DurationSeconds);
		stream.print(F("\tRSSI <- "));
		stream.println(Quality.RxRssi);
		stream.print(F("\tRSSI -> "));
		stream.println(Quality.TxRssi);

		stream.print(F("\tRx: "));
		stream.println(Quality.RxDrop);
		stream.print(F("\tRx Drop Rate: "));
		stream.println(RxDropRate);
		stream.print(F("\tTx: "));
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
			stream.print(F(" days "));
		}
		else if (durationDays > 0)
		{
			stream.print(durationDays);
			stream.print(F(" days "));
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
#endif