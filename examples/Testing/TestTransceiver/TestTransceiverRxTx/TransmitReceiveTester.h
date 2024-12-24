// TransmitReceiveTester.h

#ifndef _TRASMIT_RECEIVE_TESTER_h
#define _TRASMIT_RECEIVE_TESTER_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <ILoLaInclude.h>
#include <Arduino.h>

#include "Link\LoLaPacketService.h"

template<const bool TxEnabled = false,
	const uint8_t TxActivePin = UINT8_MAX>
class TransmitReceiveTester : private TS::Task, public virtual IPacketServiceListener
{
public:
	static constexpr uint8_t TestChannel = 0;

private:
	// The outgoing content is encrypted and MAC'd here before being sent to the Transceiver for transmission.
	uint8_t OutPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	// The incoming encrypted and MAC'd packet is stored, here before validated, and decrypted to InData.
	uint8_t InPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	uint32_t LastSent = 0;
	uint32_t LastReceived = 0;

	volatile uint32_t TxStartTimestamp = 0;
	volatile uint32_t TxEndTimestamp = 0;
	bool TxActive = false;
	bool SmallBig = false;

	// Packet service instance, with reference low latency timeouts.
	LoLaPacketService PacketService;

public:
	TransmitReceiveTester(Scheduler& scheduler, ILoLaTransceiver* transceiver)
		: IPacketServiceListener()
		, TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, true)
		, PacketService(scheduler, this, transceiver, InPacket, OutPacket)
	{}

	const bool Setup()
	{
		if (TxActivePin > 0)
		{
			pinMode(TxActivePin, OUTPUT);
		}

		if (PacketService.Setup())
		{
			PacketService.Transceiver->Start();

			PacketService.RefreshChannel();

			LastSent = millis();
			LastReceived = millis();

			for (size_t i = 0; i < LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE; i++)
			{
				OutPacket[i] = i;
			}

			TS::Task::enable();
			return true;
		}
		else
		{
			TS::Task::disable();
			return false;
		}
	}

	// ILoLaTransceiverListener overrides
public:
	virtual const uint8_t GetRxChannel() final
	{
		return TestChannel;
	}

	/// <summary>
	/// The Packet Service has a received packet ready to consume.
	/// </summary>
	/// <param name="receiveTimestamp">micros() timestamp of packet start.</param>
	/// <param name="data">Raw packet data is provided separately, to avoid a repeating field.</param>
	/// <param name="packetSize"></param>
	virtual void OnReceived(const uint32_t receiveTimestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		Serial.print(F("On Rx! Rssi: "));
		Serial.println(rssi);
	}

	/// <summary>
	/// The Packet driver had a packet ready for receive,
	///  but the packet service was still busy serving the last one.
	/// </summary>
	/// <param name="startTimestamp"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	virtual void OnDropped(const uint32_t startTimestamp, const uint8_t packetSize) final
	{
		Serial.println(F("On Rx Packet Drop?"));
	}

	/// <summary>
	/// The Physical Driver failed to receive an integral packet.
	/// </summary>
	/// <param name="startTimestamp"></param>
	virtual void OnLost(const uint32_t startTimestamp) final
	{
		Serial.println(F("On Rx Driver Drop?"));
	}

	/// <summary>
	/// Optional callback.
	/// Lets the Sender know the transmission is complete or failed.
	/// </summary>
	/// <param name="result"></param>
	virtual void OnSendComplete(const SendResultEnum result) final
	{
		if (TxActivePin > 0)
		{
			digitalWrite(TxActivePin, LOW);
		}

		TxActive = false;
		const uint32_t endTimestamp = micros();

		Serial.print(F("Tx took "));
		Serial.print(endTimestamp - TxStartTimestamp);
		Serial.println(F(" us"));
	}

public:
	virtual bool Callback() final
	{
		const uint32_t timestamp = millis();

		uint8_t testPacketSize = 0;
		if (SmallBig)
		{
			testPacketSize = LoLaPacketDefinition::MIN_PACKET_SIZE;
		}
		else
		{
			testPacketSize = LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE;
		}

		if (TxEnabled
			&& !TxActive
			&& ((timestamp - LastSent) >= 1000)
			&& (timestamp - LastReceived >= 500)
			&& PacketService.CanSendPacket())
		{
			TxStartTimestamp = micros();

			if (TxActivePin != UINT8_MAX)
			{
				digitalWrite(TxActivePin, HIGH);
			}

			if (PacketService.Send(testPacketSize, TestChannel))
			{
				TxEndTimestamp = micros();
				LastSent = timestamp;
				SmallBig = !SmallBig;
				OutPacket[0]++;

				TxActive = true;

				const uint32_t txDuration = micros() - TxStartTimestamp;
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

			return true;
		}
		else
		{
			TS::Task::delay(1);
			return false;
		}
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

