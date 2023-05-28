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
	static constexpr uint8_t TestPacketSize = LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE;
	uint8_t TestPacket[TestPacketSize];

	// Selected Driver for test.
	ILoLaTransceiver* Transceiver;

	uint32_t LastSentMicros = 0;

	uint32_t TxStarTimestamp = 0;


public:
	TransmissionTester(Scheduler& scheduler, ILoLaTransceiver* transceiver)
		: Task(SendPeriodMillis, TASK_FOREVER, &scheduler, true)
		, Transceiver(transceiver)
		, ILoLaTransceiverListener()
	{
		if (TxActivePin > 0)
		{
			pinMode(TxActivePin, OUTPUT);
		}
	}

	const bool Setup()
	{
		for (size_t i = 0; i < TestPacketSize; i++)
		{
			TestPacket[i] = i;
		}

		return Transceiver != nullptr && Transceiver->SetupListener(this) && Transceiver->Start();
	}

	// ILoLaTransceiverListener overrides
public:
	virtual const bool OnRx(const uint8_t* data, const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		return false;
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
		const uint32_t endTimestamp = micros();

		Serial.print(F("OnTx. Took "));
		Serial.print(endTimestamp - TxStarTimestamp);
		Serial.println(F(" us"));
	}

public:
	virtual bool Callback() final
	{
		if (Transceiver->TxAvailable())
		{
			TxStarTimestamp = micros();

			if (TxActivePin > 0)
			{
				digitalWrite(TxActivePin, HIGH);
			}

			if (Transceiver->Tx(TestPacket, TestPacketSize, 0))
			{
				/*			Serial.print(F("Tx ("));
							Serial.print(TestPacketSize);
							Serial.println(F(") bytes."));*/
			}
			else
			{
				Serial.println(F("Tx Failed."));
			}

			//if (TxActivePin > 0)
			//{
			//	digitalWrite(TxActivePin, LOW);
			//}
		}

		return true;
	}
};
#endif

