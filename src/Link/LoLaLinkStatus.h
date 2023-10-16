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
#endif
};
#endif