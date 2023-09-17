// TransmissionTester.h

#ifndef _TRASMISSION_TESTER_h
#define _TRASMISSION_TESTER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaInclude.h>
#include <Arduino.h>


template<const uint32_t SendPeriodMillis,
	const uint8_t TxActivePin = 0>
class TransmissionTester : private Task, public virtual ILoLaTransceiverListener
{
private:
	uint8_t TestPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE];

	// Selected Driver for test.
	ILoLaTransceiver* Transceiver;

	uint32_t TxStartTimestamp = 0;


	bool TxActive = false;
	bool SmallBig = false;

public:
	TransmissionTester(Scheduler& scheduler, ILoLaTransceiver* transceiver)
		: ILoLaTransceiverListener()
		, Task(SendPeriodMillis, TASK_FOREVER, &scheduler, true)
		, Transceiver(transceiver)
	{
		if (TxActivePin > 0)
		{
			pinMode(TxActivePin, OUTPUT);
		}
	}

	const bool Setup()
	{
		for (size_t i = 0; i < LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE; i++)
		{
			TestPacket[i] = i;
		}

		if (Transceiver != nullptr && Transceiver->SetupListener(this) && Transceiver->Start())
		{
			Transceiver->Rx(UINT8_MAX / 2);

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

		Transceiver->Rx(UINT8_MAX / 2);
	}

public:
	virtual bool Callback() final
	{
		if (!TxActive &&
			Transceiver->TxAvailable())
		{
			TxStartTimestamp = micros();

			if (TxActivePin > 0)
			{
				digitalWrite(TxActivePin, HIGH);
			}

			uint8_t TestPacketSize = 0;
			if (SmallBig)
			{
				TestPacketSize = LoLaPacketDefinition::MIN_PACKET_SIZE;
			}
			else
			{
				TestPacketSize = LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE;
			}

			if (Transceiver->Tx(TestPacket, TestPacketSize, UINT8_MAX / 2))
			{
				TxActive = true;
				SmallBig = !SmallBig;
				TestPacket[0]++;

				Serial.print(F("Tx ("));
				Serial.print(TestPacketSize);
				Serial.println(F(") bytes."));
			}
			else
			{
				Serial.println(F("Tx Failed."));
			}
		}
		else
		{
			Serial.println(F("Tx not available"));
			Task::delay(1000);
		}

		return true;
	}
};
#endif

