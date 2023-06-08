// TransmitReceiveTester.h

#ifndef _TRASMIT_RECEIVE_TESTER_h
#define _TRASMIT_RECEIVE_TESTER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaInclude.h>
#include <Arduino.h>


class TransmitReceiveTester : private Task, public virtual ILoLaTransceiverListener
{
private:
	static constexpr uint8_t TestPacketSize = LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE;
	uint8_t TestPacket[TestPacketSize];

	// Selected Driver for test.
	ILoLaTransceiver* Transceiver;

	uint32_t LastSent = 0;
	uint32_t LastReceived = 0;

	uint32_t TxStartTimestamp = 0;


public:
	TransmitReceiveTester(Scheduler& scheduler, ILoLaTransceiver* transceiver)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, true)
		, Transceiver(transceiver)
		, ILoLaTransceiverListener()
	{
	}

	const bool Setup()
	{
		for (size_t i = 0; i < TestPacketSize; i++)
		{
			TestPacket[i] = i;
		}

		if (Transceiver != nullptr && Transceiver->SetupListener(this) && Transceiver->Start())
		{
			Task::enable();
			return true;
		}
		else
		{
			Task::disable();
			return false;
		}
	}

	// ILoLaTransceiverListener overrides
public:
	virtual const bool OnRx(const uint8_t* data, const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		LastReceived = millis();
		Serial.print(F("OnRx ("));
		Serial.print(packetSize);
		Serial.print(F(" bytes)"));

		PrintPacket(data, packetSize);
		Serial.println();
	}

	virtual void OnRxLost(const uint32_t receiveTimestamp) final
	{
	}

	virtual void OnTx() final
	{
		const uint32_t endTimestamp = micros();

		Serial.print(F("OnTx. Took "));
		Serial.print(endTimestamp - TxStartTimestamp);
		Serial.println(F(" us"));
	}

public:
	virtual bool Callback() final
	{
		const uint32_t timestamp = millis();

		if (((timestamp - LastSent) > 1000)
			&& (timestamp - LastReceived > 500))
		{
			if (Transceiver->TxAvailable())
			{
				TxStartTimestamp = micros();
				LastSent = timestamp;

				if (Transceiver->Tx(TestPacket, TestPacketSize, 0))
				{
					Serial.print(F("Tx ("));
					Serial.print(TestPacketSize);
					Serial.println(F(") bytes."));
				}
				else
				{
					Serial.println(F("Tx Failed."));
				}
			}

			return true;
		}
		else
		{
			Task::delay(1);
			return false;
		}


		return true;
	}

private:
	void PrintPacket(const uint8_t* data, const uint8_t size)
	{
		Serial.println();
		Serial.print('|');
		Serial.print('|');

		for (uint8_t i = 0; i < size; i++)
		{
			Serial.print('0');
			Serial.print('x');
			if (data[i] < 0x10)
			{
				Serial.print(0);
			}
			Serial.print(data[i], HEX);
			Serial.print('|');
		}
		Serial.println('|');
	}
};
#endif

