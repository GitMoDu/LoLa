// TransmissionTester.h

#ifndef _TRASMISSION_TESTER_h
#define _TRASMISSION_TESTER_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <ILoLaInclude.h>
#include <Arduino.h>


template<const uint32_t SendPeriodMillis,
	const uint8_t TxActivePin = 0>
class TransmissionTester : private TS::Task, public virtual ILoLaTransceiverListener
{
public:
	static constexpr uint8_t TestChannel = 0;

private:
	uint8_t TestPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE];

	ILoLaTransceiver* Transceiver;

	uint32_t TxStartTimestamp = 0;

	bool TxActive = false;
	bool SmallBig = false;

public:
	TransmissionTester(Scheduler& scheduler, ILoLaTransceiver* transceiver)
		: ILoLaTransceiverListener()
		, TS::Task(SendPeriodMillis, TASK_FOREVER, &scheduler, true)
		, Transceiver(transceiver)
	{}

	const bool Setup()
	{
		if (TxActivePin > 0)
		{
			pinMode(TxActivePin, OUTPUT);
		}

		for (size_t i = 0; i < LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE; i++)
		{
			TestPacket[i] = i;
		}

		if (Transceiver != nullptr && Transceiver->SetupListener(this) && Transceiver->Start())
		{
			Transceiver->Rx(TestChannel);

			return true;
		}

		return false;
	}

	// ILoLaTransceiverListener overrides
public:
	virtual const bool OnRx(const uint8_t* data, const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		return true;
	}

	virtual void OnRxLost(const uint32_t receiveTimestamp) final
	{
	}

	virtual void OnTx() final
	{
		if (TxActivePin > 0)
		{
			digitalWrite(TxActivePin, LOW);
		}
		TxActive = false;
		const uint32_t endTimestamp = micros();

		Serial.print(F("OnTx. Took "));
		Serial.print(endTimestamp - TxStartTimestamp);
		Serial.println(F(" us"));

		Transceiver->Rx(TestChannel);
	}

public:
	virtual bool Callback() final
	{
		uint8_t testPacketSize = 0;

		if (!TxActive &&
			Transceiver->TxAvailable())
		{
			if (SmallBig)
			{
				testPacketSize = LoLaPacketDefinition::MIN_PACKET_SIZE;
			}
			else
			{
				testPacketSize = LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE;
			}

			TxStartTimestamp = micros();

			if (TxActivePin > 0)
			{
				digitalWrite(TxActivePin, HIGH);
			}

			if (Transceiver->Tx(TestPacket, testPacketSize, TestChannel))
			{
				const uint32_t txDuration = micros() - TxStartTimestamp;
				TxActive = true;
				SmallBig = !SmallBig;
				TestPacket[0]++;

				Serial.print(F("Tx "));
				Serial.print(testPacketSize);
				Serial.print(F(" bytes ("));
				Serial.print(txDuration);
				Serial.println(F(" us)."));
			}
			else
			{
				Serial.println(F("Tx Failed."));
			}
		}
		else
		{
			Serial.println(F("Tx not available"));
			TS::Task::delay(1000);
		}

		return true;
	}
};
#endif

