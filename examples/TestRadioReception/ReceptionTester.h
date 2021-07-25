// TransmissionTester.h

#ifndef _TRASMISSIONTESTER_h
#define _TRASMISSIONTESTER_h

#include <LoLaManagerInclude.h>

#include <Services\Link\LoLaLinkService.h>

class ReceptionTester : public LoLaLinkService
{
private:
	LoLaLinkClockSyncer ClockSyncer;

	enum TestStateEnum : uint8_t
	{
		StartingUp,
		Restarting,
		Transmitting,
	} TestState = TestStateEnum::StartingUp;

	const uint32_t ResendPeriodMillis = 100;
	const uint32_t TotalSendPeriodMillis = 2000;
	const uint32_t TotalSleepPeriodMillis = 2000;

	uint32_t CycleStartMillis = 0;

	uint32_t ReceivedCount = 0;

	const uint8_t TestId = 123;


public:
	ReceptionTester(Scheduler* scheduler, ILoLaDriver* driver)
		: LoLaLinkService(scheduler, scheduler, driver)
		, ClockSyncer()
	{
		SetClockSyncer(&ClockSyncer);
	}

	virtual bool Setup()
	{
		if (ClockSyncer.Setup(&SyncedClock) &&
			LoLaLinkService::Setup())
		{

			LastChannelChange = millis() - ChannelChangePeriodMillis;
			return true;
		}

		return false;
	}

#ifdef DEBUG_LOLA
	void PrintName(Stream* serial)
	{
		serial->print(F("Transmission Tester"));
	}
#endif // DEBUG_LOLA

	virtual bool OnEnable()
	{
		return true;
	}

	virtual void OnDisable()
	{
	}

	virtual bool OnPacketReceived(const uint8_t header, const uint8_t id, const uint32_t timestamp, uint8_t* payload)
	{
		ReceivedCount++;
#ifdef DEBUG_LOLA
		Serial.print(timestamp);
		Serial.print(F(": Received"));

		if (id == TestId)
		{
			Serial.println(F(" Transmission Tester Packet"));

			ATUI_R.array[0] = payload[0];
			ATUI_R.array[1] = payload[1];
			ATUI_R.array[2] = payload[2];
			ATUI_R.array[3] = payload[3];

			Serial.print(F("\tClocksync:"));
			Serial.println(ATUI_R.uint);

			ATUI_R.array[0] = payload[4];
			ATUI_R.array[1] = payload[5];
			ATUI_R.array[2] = payload[6];
			ATUI_R.array[3] = payload[7];

			Serial.print(F("\tTransmitter Count:"));
			Serial.println(ATUI_R.uint);
		}
		else
		{
			Serial.println(F(" Unknown Packet"));
		}

		Serial.print(F("\tTotal:"));
		Serial.println(ReceivedCount);
#endif
	}

protected:
	uint32_t LastChannelChange = 0;
	uint32_t LastSample = 0;
	uint32_t LastDebug = 0;

	const uint32_t ChannelChangePeriodMillis = 600000;
	const uint32_t SamplePeriodMillis = 2;
	const uint32_t DebugPeriodMillis = 4000;
	const uint32_t MaxSampleCount = 300;

	int32_t Sum = 0;
	uint32_t SampleCount = 0;

	int16_t AverageRSSI = AverageRSSI;
	int16_t Sample = ILOLA_INVALID_RSSI;


	virtual void OnService()
	{
		if (millis() - LastSample > SamplePeriodMillis)
		{
			LastSample = millis();

			Sample = ILOLA_INVALID_RSSI;

			if (Sample != ILOLA_INVALID_RSSI)
			{
				Sum += LoLaDriver->GetLastRSSI();
				SampleCount++;

				if (SampleCount > MaxSampleCount)
				{
					AverageRSSI = Sum / SampleCount;

					Sum = 0;
					SampleCount = 0;
				}
			}
		}

		if (millis() - LastChannelChange > ChannelChangePeriodMillis)
		{
			LastChannelChange = millis();
			ChannelSelector.NextRandomChannel();
			ChannelSelector.NextRandomChannel();
		
			LoLaDriver->OnChannelUpdated();
#ifdef DEBUG_LOLA
			Serial.print(F("Channel: "));
			Serial.println(ChannelSelector.GetChannel());
#endif	
		}

//		if (millis() - LastDebug > DebugPeriodMillis)
//		{
//			LastDebug = millis();
//
//#ifdef DEBUG_LOLA
//			if (AverageRSSI != ILOLA_INVALID_RSSI)
//			{
//				Serial.print(F("Average RSSI: "));
//				Serial.println(AverageRSSI);
//			}
//#endif
//		}
	}
};
#endif

