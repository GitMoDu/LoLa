// VirtualHalfDuplexDriver.h

#ifndef _VIRTUAL_HALF_DUPLEX_DRIVER_h
#define _VIRTUAL_HALF_DUPLEX_DRIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaPacketDriver.h>
#include "IVirtualPacketDriver.h"

/// <summary>
/// Virtual Packet Driver.
/// Provides a double-buffered simulated PHY, for LoLaLinks.
/// Can send and transmit to IVirtualPacketDriver partner.
/// </summary>
/// <typeparam name="Config">IVirtualPacketDriver::Configuration: Simulated PHY characteristics.</typeparam>
/// <typeparam name="OnwerName">Indentifier for debug logging.</typeparam>
/// <typeparam name="LogChannelHop">Enables current channel logging, when DEBUG_LOLA is defined.</typeparam>
template<typename Config,
	const char OnwerName,
	const bool LogChannelHop = false,
	const uint8_t PinTestTx = 0>
class VirtualHalfDuplexDriver
	: private Task
	, public virtual IVirtualPacketDriver
	, public virtual ILoLaPacketDriver
{
private:
	template<const uint8_t MaxPacketSize>
	struct IoPacketStruct
	{
		uint8_t Buffer[MaxPacketSize]{};
		uint32_t Started = 0;
		uint8_t Size = 0;

		const bool HasPending()
		{
			return Size > 0;
		}

		virtual void Clear()
		{
			Size = 0;
		}
	};

	template<const uint8_t MaxPacketSize>
	struct OutPacketStruct : public IoPacketStruct<MaxPacketSize>
	{
		uint8_t Channel = 0;
	};

private:
	ILoLaPacketDriverListener* PacketListener = nullptr;

private:
	IVirtualPacketDriver* Partner = nullptr;


private:
	IoPacketStruct<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE> Incoming{};
	OutPacketStruct<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE> OutGoing{};

	uint8_t CurrentChannel = 0;

	bool DriverEnabled = false;

private:
	void PrintName()
	{
#ifdef DEBUG_LOLA
		Serial.print('{');
		Serial.print(OnwerName);
		Serial.print('}');
		Serial.print('\t');
#endif
	}

public:
	VirtualHalfDuplexDriver(Scheduler& scheduler)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, IVirtualPacketDriver()
		, ILoLaPacketDriver()
	{
	}

public:
	virtual bool Callback() final
	{
		// Simulate transmit delay, from request to on-air start.
		if (OutGoing.HasPending() && (micros() - OutGoing.Started > GetTransmitDurationMicros(OutGoing.Size)))
		{
			// Tx duration has elapsed.
			UpdateChannel(OutGoing.Channel);
			Partner->ReceivePacket(OutGoing.Buffer, OutGoing.Size, CurrentChannel);
			OutGoing.Clear();
			if (PacketListener != nullptr)
			{
				PacketListener->OnSent(true);
			}
		}

		// Simulate delay from start event to received packet buffer.
		if (Incoming.HasPending() && (micros() - Incoming.Started > GetRxDuration(Incoming.Size)))
		{
			// Rx duration has elapsed since the packet incoming start triggered.
			if (PacketListener != nullptr)
			{
				if (!PacketListener->OnReceived(Incoming.Buffer, Incoming.Started, Incoming.Size, UINT8_MAX / 2))
				{
#if defined(DEBUG_LOLA)
					Serial.print(millis());
					Serial.print('\t');
					PrintName();
					Serial.println(F("Rx Collision. Packet Service rejected."));
#endif
				}
			}
			Incoming.Clear();
		}

		if (OutGoing.HasPending() || Incoming.HasPending())
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

public:
	virtual const bool DriverSetupListener(ILoLaPacketDriverListener* packetListener) final
	{
		PacketListener = packetListener;

		return PacketListener != nullptr;
	}

	virtual const bool DriverStart() final
	{
		Incoming.Clear();
		OutGoing.Clear();

		CurrentChannel = 0;
		DriverEnabled = true;

		Task::enable();

		return true;
	}

	virtual const bool DriverStop() final
	{
		DriverEnabled = false;

		Task::disable();

		return true;
	}

	virtual const bool DriverTx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
	{
		const uint32_t timestamp = micros();

		if (!DriverEnabled)
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Tx failed. Driver disabled."));
#endif
			return false;
		}

		if (packetSize < LoLaPacketDefinition::MIN_PACKET_SIZE || packetSize > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.print(F("Tx failed. Invalid packet Size ("));
			Serial.print(packetSize);
			Serial.println(F(") bytes."));
#endif
			return false;
		}

		if (OutGoing.HasPending())
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Tx failed. Was sending."));
#endif
			return false;
		}

		OutGoing.Size = packetSize;
		OutGoing.Channel = channel;
		for (uint_fast8_t i = 0; i < packetSize; i++)
		{
			OutGoing.Buffer[i] = data[i];
		}
		OutGoing.Started = micros();

		Task::enable();

		return true;
	}

	virtual void DriverRx(const uint8_t channel) final
	{
		UpdateChannel(channel);
	}

	virtual const uint32_t GetTransmitDurationMicros(const uint8_t packetSize) final
	{
		return Config::TxBase + ((Config::TxByteNanos * packetSize) / 1000);
	}

	virtual const bool DriverCanTransmit() final
	{
		return DriverEnabled && !OutGoing.HasPending() && !Incoming.HasPending();
	}

public:
	virtual void SetPartner(IVirtualPacketDriver* partner) final
	{
		Partner = partner;
	}

	virtual void ReceivePacket(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
	{
		const uint32_t timestamp = micros();

		if (Incoming.HasPending())
		{
			if (PacketListener != nullptr)
			{
				PacketListener->OnReceiveLost(timestamp);
			}
#if defined(DEBUG_LOLA)
			Serial.print(millis());
			Serial.print('\t');
			PrintName();
			Serial.println(F("Virtual Rx Collision."));
#endif
			return;
		}

		if (CurrentChannel != channel)
		{
#if defined(DEBUG_LOLA)
			Serial.print(millis());
			Serial.print('\t');
			PrintName();
			Serial.print(F("Channel Miss: Rx:"));
			Serial.print(CurrentChannel);
			Serial.print(F(" Tx:"));
			Serial.println(channel);
#endif
			return;
		}

		// Copy packet to temporary local copy of partner output.
		// This will be distributed as Incoming after TransmitDuration,
		// simulating the transmit delay without async calls.
		Incoming.Started = timestamp;
		Incoming.Size = packetSize;
		for (uint_fast8_t i = 0; i < packetSize; i++)
		{
			Incoming.Buffer[i] = data[i];
		}

#if defined(CORRUPT_CHANCE)
		if (random(UINT8_MAX) < CORRUPT_CHANCE)
		{
			Incoming.Buffer[random(packetSize - 1)] = random(UINT8_MAX);
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Corruption attack!"));
#endif
		}
#endif
		Task::enable();
	}

	const uint32_t GetRxDuration(const uint8_t size)
	{
		return Config::RxBase + ((Config::RxByteNanos * size)) / 1000;
	}

private:
	void UpdateChannel(const uint8_t channel)
	{
		if (CurrentChannel != (channel % Config::ChannelCount))
		{
			CurrentChannel = channel % Config::ChannelCount;
			if (LogChannelHop)
			{
				LogChannel();
			}
		}
	}

	void LogChannel()
	{
		LogChannel(CurrentChannel, Config::ChannelCount, 1);
	}

	void LogChannel(const uint8_t channel, const uint8_t channelCount, const uint8_t channelDivisor)
	{
#if defined(DEBUG_LOLA)
		const uint_fast8_t channelScaled = channel % channelCount;

		for (uint_fast8_t i = 0; i < channelScaled; i++)
		{
			Serial.print('_');
		}


		Serial.print('/');
		Serial.print('\\');

		for (uint_fast8_t i = (channelScaled + 1); i < channelCount; i++)
		{
			Serial.print('_');
		}

		Serial.println();
#endif
	}

#if defined(PRINT_PACKETS)
	void PrintPacket(uint8_t* data, const uint8_t size)
	{
		Serial.println();
		Serial.print('|');
		Serial.print('|');

		uint32_t mac = data[0];
		mac += data[1] << 8;
		mac += (uint32_t)data[2] << 16;
		mac += (uint32_t)data[3] << 24;
		Serial.print(mac);
		Serial.print('|');

		Serial.print(data[4]);
		Serial.print('|');

		for (uint8_t i = 5; i < size; i++)
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
#endif
};
#endif