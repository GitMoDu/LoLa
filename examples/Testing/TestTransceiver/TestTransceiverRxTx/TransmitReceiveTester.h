// TransmitReceiveTester.h

#ifndef _TRASMIT_RECEIVE_TESTER_h
#define _TRASMIT_RECEIVE_TESTER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaInclude.h>
#include <Arduino.h>

#include "Link\LoLaPacketService.h"

template<const bool TxEnabled = false>
class TransmitReceiveTester : private Task, public virtual IPacketServiceListener
{
public:
	static constexpr uint8_t TestChannel = UINT8_MAX / 2;

private:
	static constexpr uint8_t TestPacketSize = LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE;

	// The outgoing content is encrypted and MAC'd here before being sent to the Transceiver for transmission.
	uint8_t OutPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	// The incoming encrypted and MAC'd packet is stored, here before validated, and decrypted to InData.
	uint8_t InPacket[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	uint32_t LastSent = 0;
	uint32_t LastReceived = 0;

	volatile uint32_t TxStartTimestamp = 0;
	volatile uint32_t TxEndTimestamp = 0;
	bool TxActive = false;

	// Packet service instance, with reference low latency timeouts.
	LoLaPacketService PacketService;

public:
	TransmitReceiveTester(Scheduler& scheduler, ILoLaTransceiver* transceiver)
		: IPacketServiceListener()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, true)
		, PacketService(scheduler, this, transceiver, InPacket, OutPacket)
	{}

	const bool Setup()
	{
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

		if (TxEnabled 
			&& !TxActive
			&& ((timestamp - LastSent) >= 1000)
			&& (timestamp - LastReceived >= 500)
			&& PacketService.CanSendPacket()
			)
		{
			Serial.print(F(": Tx."));

			OutPacket[0]++;

			TxStartTimestamp = micros();
			if (PacketService.Send(TestPacketSize, TestChannel))
			{
				TxEndTimestamp = micros();
				LastSent = timestamp;
				TxActive = true;
			}
			else
			{
				Serial.println(F("Tx Failed."));
			}

			return true;
		}
		else
		{
			Task::delay(1);
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

