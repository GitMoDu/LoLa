// TransmissionTester.h

#ifndef _TRASMISSIONTESTER_h
#define _TRASMISSIONTESTER_h

#include <LoLaManagerInclude.h>

#include <Services\Link\LoLaLinkService.h>

class TransmissionTester : public LoLaLinkService
{
private:
	LoLaLinkClockSyncer ClockSyncer;

	const uint8_t TestId = 123;

	enum TestStateEnum : uint8_t
	{
		StartingUp,
		Restarting,
		Transmitting,
	} TestState = TestStateEnum::StartingUp;

	const uint32_t ResendPeriodMillis = 10;
	const uint32_t TotalSendPeriodMillis = 20;
	const uint32_t TotalSleepPeriodMillis = 1;

	uint32_t CycleStartMillis = 0;

	uint32_t TransmittedCount = 0;

public:
	TransmissionTester(Scheduler* scheduler, ILoLaDriver* driver)
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
		// Ignore incoming packets.
	}


private:
	void PrepareTestPacket()
	{
		OutPacket.SetDefinition(&DefinitionMulti);
		OutPacket.SetId(TestId);

		ATUI_S.uint = (SyncedClock.GetSyncMicros() + LoLaDriver->GetETTMMicros()) % UINT32_MAX;

		OutPacket.GetPayload()[0] = ATUI_S.array[0];
		OutPacket.GetPayload()[1] = ATUI_S.array[1];
		OutPacket.GetPayload()[2] = ATUI_S.array[2];
		OutPacket.GetPayload()[3] = ATUI_S.array[3];

		ATUI_S.uint = TransmittedCount;

		OutPacket.GetPayload()[4] = ATUI_S.array[0];
		OutPacket.GetPayload()[5] = ATUI_S.array[1];
		OutPacket.GetPayload()[6] = ATUI_S.array[2];
		OutPacket.GetPayload()[7] = ATUI_S.array[3];
	}

protected:
	void OnSendOk()
	{
		TransmittedCount++;
		//#ifdef DEBUG_LOLA
		//		Serial.print(F("Sent Packet! ("));
		//		Serial.print(TransmittedCount);
		//		Serial.println(')');
		//#endif
	}

	uint8_t NewChannel = 0;

	virtual void OnService()
	{
		switch (TestState)
		{
		case TestStateEnum::StartingUp:
			LoLaDriver->Start();
			CryptoEncoder.Clear();
			TestState = TestStateEnum::Restarting;
			SetNextRunASAP();
			break;
		case TestStateEnum::Restarting:
			TestState = TestStateEnum::Transmitting;
			NewChannel = ChannelSelector.NextChannel();
			LoLaDriver->OnChannelUpdated();
			PowerBalancer.SetMaxPower();
			LoLaDriver->OnTransmitPowerUpdated();
#ifdef DEBUG_LOLA
			Serial.print(F("Channel: "));
			Serial.println(NewChannel);
#endif	
			SetNextRunASAP();
			CycleStartMillis = millis();
			break;
		case TestStateEnum::Transmitting:
			if (millis() - LastSentMillis > ResendPeriodMillis)
			{
				LastSentMillis = millis();
				PrepareTestPacket();
				RequestSendPacket();
			}
			else if (millis() - CycleStartMillis > TotalSendPeriodMillis) {
				TestState = TestStateEnum::Restarting;
				SetNextRunDelay(TotalSleepPeriodMillis);
			}
			break;
		default:
			break;
		}
	}
};
#endif

