//// LinkDebugTask.h
//
//#ifndef _LINKDEBUGTASK_h
//#define _LINKDEBUGTASK_h
//
//#define _TASK_OO_CALLBACKS
//#include <TaskSchedulerDeclarations.h>
//
//#include <LoLaDefinitions.h>
//#include <LoLaClock\LoLaClock.h>
//
//
//class LinkDebugTask : Task
//{
//private:
//	uint32_t NextDebug = 0;
//
//	uint32_t DiffieHellmanKeyExchangeDuration = 0;
//	uint32_t LinkingDuration = 0;
//
//	LoLaLinkInfo* LinkInfo = nullptr;
//	LoLaPacketDriver* LoLaDriver = nullptr;
//
//	ISyncedClock* SyncedClock = nullptr;
//	LoLaCryptoKeyExchanger* KeyExchanger = nullptr;
//
//	const static uint32_t UPDATE_PERIOD = LOLA_LINK_DEBUG_UPDATE_SECONDS * 1000;
//
//	uint32_t ETTM = 0;
//	uint32_t RTT = 0;
//
//	class EncoderDurations
//	{
//	public:
//		uint32_t ShortEncode = 0;
//		uint32_t LongEncode = 0;
//
//		uint32_t ShortDecode = 0;
//		uint32_t LongDecode = 0;
//	};
//
//	class RadioDurations
//	{
//	public:
//		uint32_t ShortTransmit = 0;
//		uint32_t LongTransmit = 0;
//	};
//
//	EncoderDurations EncoderTimings;
//	RadioDurations RadioTimings;
//
//
//	const uint32_t MeasureTimeoutMillis = 30000;
//
//	PingService LatencyService;
//
//	TemplateLoLaPacket<PacketDefinition::LOLA_PACKET_MAX_PACKET_TOTAL_SIZE> TestPacket;
//
//	PacketDefinition TestShortDefinition;
//	PacketDefinition TestLongDefinition;
//
//
//public:
//	LinkDebugTask(Scheduler* scheduler,
//		LoLaPacketDriver* loLaDriver,
//		LoLaLinkInfo* linkInfo,
//		ISyncedClock* syncedClock,
//		LoLaCryptoKeyExchanger* keyExchanger)
//		: Task(1, TASK_FOREVER, scheduler, false)
//		, LatencyService(scheduler, loLaDriver)
//		, TestShortDefinition(&LatencyService, 0, 0)
//		, TestLongDefinition(&LatencyService, 0, 0)
//	{
//		LoLaDriver = loLaDriver;
//		LinkInfo = linkInfo;
//		SyncedClock = syncedClock;
//		KeyExchanger = keyExchanger;
//	}
//
//	void Start()
//	{
//		LatencyService.Setup();
//		disable();
//	}
//
//	void Reset()
//	{
//		RTT = 0;
//		ETTM = 0;
//	}
//
//	void DebugTimings()
//	{
//		Serial.println("Radio timings");
//
//		Serial.print(F("Round Trip Time: "));
//		Serial.print(RTT);
//		Serial.println(F(" us"));
//
//		Serial.print(" ETTM: ");
//		Serial.print(ETTM);
//		Serial.println(" us");
//
//		Serial.print(" ShortEncode: ");
//		Serial.print(EncoderTimings.ShortEncode);
//		Serial.println(" us");
//
//		Serial.print(" LongEncode: ");
//		Serial.print(EncoderTimings.LongEncode);
//		Serial.println(" us");
//
//		Serial.print(" ShortDecode: ");
//		Serial.print(EncoderTimings.ShortDecode);
//		Serial.println(" us");
//
//		Serial.print(" LongDecode: ");
//		Serial.print(EncoderTimings.LongDecode);
//		Serial.println(" us");
//
//		Serial.print(" ShortTransmit + Encode: ");
//		Serial.print(RadioTimings.ShortTransmit);
//		Serial.println(" us");
//
//		Serial.print(" LongTransmit + Encode: ");
//		Serial.print(RadioTimings.LongTransmit);
//		Serial.println(" us");
//	}
//
//	void UpdateETTM()
//	{
//		const uint8_t Factor = 10;
//
//		// ETTM is biased 8/10 towards the short transmit.
//		// Short packets are much more common than long ones.
//		// This way, we're reducing our average error without adding any further runtime calculations.
//		//ETTM = (RadioTimings.ShortTransmit * 8) / (Factor);
//		//ETTM += (RadioTimings.LongTransmit * 2) / (Factor);
//
//		ETTM = ((RadioTimings.ShortTransmit * 8) + (RadioTimings.LongTransmit * 2)) / (Factor);
//	}
//
//	const uint16_t SampleCount = 100;
//
//	bool MeasureRadioTimings()
//	{
//		const uint32_t Timeout = micros() + (MeasureTimeoutMillis * 1000);
//		//uint64_t Measure = 0;
//		//uint64_t Sum = 0;
//		//uint16_t i = 0;
//		//// Measure send for short packets.
//		//TestPacket.SetHeader(&TestShortDefinition);
//
//
//		//while (i < SampleCount)
//		//{
//		//	//Measure = micros();
//		//	Measure = SyncedClock->GetSyncMicros();
//
//		//	if (LoLaDriver->SendPacketTest(&TestPacket))
//		//	{
//		//		//Sum += micros() - Measure;
//		//		Sum += SyncedClock->GetSyncMicros() - Measure;
//		//		i++;
//		//	}
//		//	else
//		//	{
//		//		Serial.println(F("Rejected."));
//		//	}
//		//}
//		//RadioTimings.ShortTransmit = Sum / SampleCount;
//		//Sum = 0;
//		//i = 0;
//		//// Measure send for long packets.
//		//TestPacket.SetDefinition(&TestLongDefinition);
//		//while (i < SampleCount)
//		//{
//		//	//Measure = micros();
//		//	Measure = SyncedClock->GetSyncMicros();
//		//	if (LoLaDriver->SendPacketTest(&TestPacket))
//		//	{
//		//		//Sum += micros() - Measure;
//		//		Sum += SyncedClock->GetSyncMicros() - Measure;
//		//		i++;
//		//	}
//		//	else
//		//	{
//		//		Serial.println(F("Rejected 2."));
//		//	}
//		//}
//
//		//RadioTimings.LongTransmit = Sum / SampleCount;
//		//Sum = 0;
//
//		return micros() < Timeout;
//	}
//
//	bool MeasureEncoderTimings()
//	{
//		const uint32_t Timeout = micros() + (MeasureTimeoutMillis * 1000);
//		uint32_t Measure = 0;
//		uint64_t Sum = 0;
//
//		// Measure CRC and encode for short packets.
//		for (uint16_t i = 0; i < SampleCount; i++)
//		{
//			Measure = micros();
//			LoLaDriver->CryptoEncoder.Encode(TestPacket.GetContent(), PacketDefinition::LOLA_PACKET_MIN_PACKET_TOTAL_SIZE, TestPacket.GetContent());
//			Sum += micros() - Measure;
//		}
//		EncoderTimings.ShortEncode = Sum / SampleCount;
//		Sum = 0;
//
//		// Measure CRC and encode for long packets.
//		for (uint16_t i = 0; i < SampleCount; i++)
//		{
//			Measure = micros();
//			LoLaDriver->CryptoEncoder.Encode(TestPacket.GetContent(), PacketDefinition::LOLA_PACKET_MAX_PACKET_TOTAL_SIZE, TestPacket.GetContent());
//			Sum += micros() - Measure;
//		}
//		EncoderTimings.LongEncode = Sum / SampleCount;
//
//		for (uint16_t i = 0; i < SampleCount; i++)
//		{
//			// Measure CRC and decode for short packets.
//			Measure = micros();
//			LoLaDriver->CryptoEncoder.Encode(TestPacket.GetContent(), PacketDefinition::LOLA_PACKET_MIN_PACKET_TOTAL_SIZE, TestPacket.GetContent());
//			Sum += micros() - Measure;
//		}
//		EncoderTimings.ShortDecode = Sum / SampleCount;
//
//		for (uint16_t i = 0; i < SampleCount; i++)
//		{
//			// Measure CRC and decode for long packets.
//			Measure = micros();
//			LoLaDriver->CryptoEncoder.Encode(TestPacket.GetContent(), PacketDefinition::LOLA_PACKET_MAX_PACKET_TOTAL_SIZE, TestPacket.GetContent());
//			Sum += micros() - Measure;
//		}
//		EncoderTimings.LongDecode = Sum / SampleCount;
//
//		return micros() < Timeout;
//	}
//
//	void DebugOut()
//	{
//		//Serial.print(F("Local MAC: "));
//		//LinkInfo->PrintMac(&Serial);
//		//Serial.println();
//		//Serial.print(F("\tId: "));
//		//Serial.println(LinkInfo->GetLocalId());
//
//		Serial.println(F("LoLa Link secured with"));
//		Serial.print(F("160 bit "));
//		KeyExchanger->Debug(&Serial);
//		Serial.println();
//		Serial.print(F("\tEncrypted with 128 bit cypher "));
//		LoLaDriver->CryptoEncoder.Debug(&Serial);
//		Serial.println();
//#ifdef LOLA_LINK_USE_CHANNEL_HOP
//		Serial.print(F("\tChannel hopping"));
//		Serial.print(LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS);
//		Serial.println(F(" ms"));
//#endif
//#ifdef LOLA_LINK_USE_TOKEN_HOP
//		Serial.print(F("\tProtected with 32 bit TOTP @ "));
//		Serial.print(LOLA_LINK_SERVICE_LINKED_TOKEN_HOP_PERIOD_MILLIS);
//		Serial.println(F(" ms"));
//#else
//		Serial.println(F("Link unsecured."));
//#endif
//
//#ifdef LOLA_LINK_ACTIVITY_LED
//		pinMode(LOLA_LINK_ACTIVITY_LED, OUTPUT);
//#endif
//	}
//
//	enum LinkStateEnum
//	{
//		Disabled = 0,
//		SetupRadio = 1,
//		AwaitingLink = 2,
//		AwaitingSleeping = 3,
//		Linking = 4,
//		Linked = 5
//	};
//
//	void OnLinkStateChanged(const uint8_t linkState)
//	{
//		Serial.print(F("New radio state: "));
//
//		switch (linkState)
//		{
//		case LinkStateEnum::Disabled:
//			Serial.println(F("Disabled"));
//			break;
//		case LinkStateEnum::SetupRadio:
//			Serial.println(F("SetupRadio"));
//			break;
//		case LinkStateEnum::AwaitingLink:
//			Serial.println(F("AwaitingLink"));
//			if (ETTM == 0)
//			{
//				if (MeasureEncoderTimings() && MeasureRadioTimings())
//				{
//					UpdateETTM();
//
//					DebugTimings();
//					DebugState = DebugStateEnum::MeasuringTimings;
//				}
//				else
//				{
//					DebugState = DebugStateEnum::FailedTime;
//				}
//			}
//			else
//			{
//				DebugState = DebugStateEnum::RuntimeDebug;
//			}
//			break;
//		case LinkStateEnum::AwaitingSleeping:
//			Serial.println(F("AwaitingSleeping"));
//			break;
//		case LinkStateEnum::Linking:
//			Serial.println(F("Linking"));
//			break;
//		case LinkStateEnum::Linked:
//			Serial.println(F("Linked"));
//			break;
//		default:
//			break;
//		}
//	}
//
//	/*void OnLinkOn()
//	{
//		LinkingDuration = millis() - LinkingDuration;
//		DebugLinkEstablished();
//		enable();
//	}
//
//	void OnLinkOff()
//	{
//		disable();
//		DebugLinkStatistics(&Serial);
//	}*/
//
//	void OnDHStarted()
//	{
//		DiffieHellmanKeyExchangeDuration = millis();
//	}
//
//	void OnDHDone()
//	{
//		DiffieHellmanKeyExchangeDuration = millis() - DiffieHellmanKeyExchangeDuration;
//	}
//
//	void OnLinkingStarted()
//	{
//		LinkingDuration = millis();
//		Serial.print(F("Linking: "));
//		Serial.print(F("Linking to Id: "));/*
//		Serial.print(LinkInfo->GetPartnerId());
//		Serial.print(F("\tSession: "));
//		Serial.println(LinkInfo->GetSessionId());*/
//		Serial.print(F("Diffie-Hellman Key Exchange took "));
//		Serial.print(DiffieHellmanKeyExchangeDuration);
//		Serial.println(F(" ms."));
//	}
//
//	bool OnEnable()
//	{
//		return LinkInfo != nullptr && LoLaDriver != nullptr;
//	}
//
//	enum DebugStateEnum
//	{
//		Starting,
//		MeasuringTimings,
//		MeasuringLatency,
//		RuntimeDebug,
//		FailedTime
//	} DebugState = DebugStateEnum::Starting;
//
//	uint32_t LatencyStartedMillis = 0;
//	uint32_t LatencyTimeoutMillis = 2000;
//
//	bool Callback()
//	{
//		switch (DebugState)
//		{
//		case DebugStateEnum::Starting:
//			DebugState = DebugStateEnum::MeasuringTimings;
//			break;
//		case DebugStateEnum::MeasuringTimings:
//			LatencyService.Start();
//			LatencyStartedMillis = millis();
//			DebugState = DebugStateEnum::MeasuringLatency;
//			break;
//		case DebugStateEnum::MeasuringLatency:
//			if (LatencyService.HasRTT())
//			{
//				DebugState = DebugStateEnum::RuntimeDebug;
//				DebugOut();
//			}
//			else
//			{
//				if (millis() - LatencyStartedMillis > LatencyTimeoutMillis)
//				{
//					DebugState = DebugStateEnum::FailedTime;
//				}
//				else
//				{
//					Task::delay(10);
//				}
//			}
//			break;
//		case DebugStateEnum::RuntimeDebug:
//			DebugLinkStatistics(&Serial);
//			Task::delay(UPDATE_PERIOD);
//			break;
//		default:
//			Serial.println(F("Debug Task failed."));
//			disable();
//			break;
//		}
//
//
//		return true;
//	}
//
//private:
//	void DebugLinkEstablished()
//	{
//		Serial.print(F("Linked: "));
//		Serial.println(SyncedClock->GetSyncSeconds());
//		Serial.print(F(" @ SyncMicros: "));
//		Serial.println((uint32_t)SyncedClock->GetSyncMicros());
//		Serial.print(F("Linking took "));
//		Serial.print(LinkingDuration);
//		Serial.println(F(" ms."));
//		Serial.print(F("Latency compensation: "));
//		Serial.print(LoLaDriver->IOInfo.ETTM);
//		Serial.println(F(" us"));
//	}
//
//	void DebugLinkStatistics(Stream* serial)
//	{
//		serial->println();
//		serial->println(F("Link Info"));
//
//		uint32_t AliveSeconds = LinkInfo->GetLinkDurationSeconds();
//
//		if (AliveSeconds / 86400 > 0)
//		{
//			serial->print(AliveSeconds / 86400);
//			serial->print(F("d "));
//			AliveSeconds = AliveSeconds % 86400;
//		}
//
//		if (AliveSeconds / 3600 < 10)
//			serial->print('0');
//		serial->print(AliveSeconds / 3600);
//		serial->print(':');
//		if ((AliveSeconds % 3600) / 60 < 10)
//			serial->print('0');
//		serial->print((AliveSeconds % 3600) / 60);
//		serial->print(':');
//		if ((AliveSeconds % 3600) % 60 < 10)
//			serial->print('0');
//		serial->println((AliveSeconds % 3600) % 60);
//
//		/*serial->print(F("ClockSync: "));
//		serial->println(LoLaDriver->GetClockSource()->GetSyncSeconds());
//		serial->print(F(" @ Offset: "));
//		serial->println(LoLaDriver->GetClockSource()->GetSyncMicros());*/
//
//
//		serial->print(F("Transmit Power: "));
//		serial->print((float)(((LoLaDriver->TransmitPowerManager.GetTransmitPowerNormalized() * 100) / UINT8_MAX)), 0);
//		serial->println(F(" %"));
//
//		serial->print(F("RSSI: "));
//		serial->print((float)(((LoLaDriver->IOInfo.GetRSSINormalized() * 100) / UINT8_MAX)), 0);
//		serial->println(F(" %"));
//		serial->print(F("RSSI Partner: "));
//		serial->print((float)(((LinkInfo->GetPartnerRSSINormalized() * 100) / UINT8_MAX)), 0);
//		serial->println(F(" %"));
//
//		/*serial->print(F("Sent: "));
//		serial->println((uint32_t)LoLaDriver->TransmitedCount);
//		serial->print(F("Lost: "));
//		serial->println((uint32_t)LinkInfo->GetLostCount());
//		serial->print(F("Received: "));
//		serial->println((uint32_t)LoLaDriver->GetReceivedCount());
//		serial->print(F("Rejected: "));
//		serial->print((uint32_t)LoLaDriver->GetRejectedCount());
//
//		serial->print(F(" ("));
//		if (LoLaDriver->GetReceivedCount() > 0)
//		{
//			serial->print((float)(LoLaDriver->GetRejectedCount() * 100) / (float)LoLaDriver->GetReceivedCount(), 2);
//			serial->println(F(" %)"));
//		}
//#ifdef DEBUG_LOLA
//		else if (LoLaDriver->GetRejectedCount() > 0)
//		{
//			serial->println(F("100 %)"));
//		}
//#endif
//		else
//		{
//			serial->println(F("0 %)"));
//		}
//		serial->print(F("Timming Collision: "));
//		serial->println((uint32_t)LoLaDriver->GetTimingCollisionCount());*/
//
//		//serial->print(F("ClockSync adjustments: "));
//		//serial->println(LinkInfo->GetClockSyncAdjustments());
//		//serial->println();
//	}
//};
//
//
//#endif
//
