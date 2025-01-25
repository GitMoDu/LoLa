// ReceptionTester.h

#ifndef _RECEPTION_TESTER_h
#define _RECEPTION_TESTER_h

#include <ILoLaInclude.h>
#include <Arduino.h>

class ReceptionTester : public virtual ILoLaTransceiverListener
{
public:
	static constexpr uint8_t TestChannel = 40;

private:
	uint8_t TestPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE];

	ILoLaTransceiver* Transceiver;

public:
	ReceptionTester(ILoLaTransceiver* transceiver)
		: ILoLaTransceiverListener()
		, Transceiver(transceiver)
	{}

	const bool Setup()
	{
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
		Transceiver->Rx(TestChannel);

		Serial.print(F("OnRx ("));
		Serial.print(packetSize);
		Serial.print(F(" bytes)"));

		Serial.print(F(" RSSI ("));
		Serial.print(rssi);
		Serial.print(F("/255)"));

		PrintPacket(data, packetSize);
		Serial.println();

		return true;
	}

	virtual void OnRxFail() final
	{
		Serial.println(F("OnRxLost."));
	}

	virtual void OnTx() final
	{
		Transceiver->Rx(TestChannel);
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

