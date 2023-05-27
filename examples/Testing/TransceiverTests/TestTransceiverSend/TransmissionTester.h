// TransmissionTester.h

#ifndef _TRASMISSIONTESTER_h
#define _TRASMISSIONTESTER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaInclude.h>
#include <Arduino.h>




template<const uint32_t SendPeriodMillis,
	const uint8_t TxActivePin = 0>
class TransmissionTester : private Task
{
private:
	uint8_t TestPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE];

	// Selected Driver for test.
	ILoLaTransceiver* Transceiver;

	uint32_t LastSentMicros = 0;

public:
	TransmissionTester(Scheduler& scheduler, ILoLaTransceiver* transceiver)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, true)
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

		return Transceiver != nullptr && Transceiver->Start();
	}
public:
	virtual bool Callback() final
	{
		if ((micros() - LastSentMicros) > (SendPeriodMillis * 1000L))
		{
			if (Transceiver->TxAvailable())
			{
				if (TxActivePin > 0)
				{
					digitalWrite(TxActivePin, HIGH);
				}

				if (Transceiver->Tx(TestPacket, LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE, 0))
				{
					LastSentMicros = micros();
					Serial.println(F("Packet Sent."));
				}

				if (TxActivePin > 0)
				{
					digitalWrite(TxActivePin, LOW);
				}
			}
		}

		return true;
	}
};
#endif

