// ReceptionTester.h

#ifndef _RECEPTION_TESTER_h
#define _RECEPTION_TESTER_h


#include <ILoLaInclude.h>
#include <Arduino.h>


template<const uint8_t RxActivePin = 0>
class ReceptionTester : public virtual ILoLaTransceiverListener
{
private:
	uint8_t TestPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE];

	// Selected Driver for test.
	ILoLaTransceiver* Transceiver;

	uint32_t LastSentMicros = 0;

	uint32_t TxStarTimestamp = 0;


public:
	ReceptionTester(ILoLaTransceiver* transceiver)
		: Transceiver(transceiver)
		, ILoLaTransceiverListener()
	{
		if (RxActivePin > 0)
		{
			pinMode(RxActivePin, OUTPUT);
		}
	}

	const bool Setup()
	{
		for (size_t i = 0; i < LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE; i++)
		{
			TestPacket[i] = i;
		}

		return Transceiver != nullptr && Transceiver->SetupListener(this) && Transceiver->Start();
	}

	// ILoLaTransceiverListener overrides
public:
	virtual const bool OnRx(const uint8_t* data, const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		if (RxActivePin > 0)
		{
			digitalWrite(RxActivePin, HIGH);
		}

		Serial.print(F("OnRx ("));
		Serial.print(packetSize);
		Serial.print(F(" bytes)"));

		PrintPacket(data, packetSize);
		Serial.println();

		return true;
	}

	virtual void OnRxLost(const uint32_t receiveTimestamp) final
	{
		Serial.println(F("OnRxLost."));
	}

	virtual void OnTx() final
	{
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

