// VirtualTransceiver.h

#ifndef _VIRTUAL_TRANSCEIVER_h
#define _VIRTUAL_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>
#include "IVirtualTransceiver.h"

/// <summary>
/// Virtual Packet Transceiver.
/// Provides a double-buffered simulated PHY, for LoLaLinks.
/// Can send and transmit to IVirtualTransceiver partner.
/// </summary>
/// <typeparam name="Config">IVirtualTransceiver::Configuration: Simulated PHY characteristics.</typeparam>
/// <typeparam name="OnwerName">Indentifier for debug logging.</typeparam>
/// <typeparam name="LogChannelHop">Enables current channel logging, when DEBUG_LOLA is defined.</typeparam>
template<typename Config,
	const char OnwerName,
	const bool LogChannelHop = false,
	const uint8_t PinTestTx = 0>
class VirtualTransceiver final
	: private Task
	, public virtual IVirtualTransceiver
	, public virtual ILoLaTransceiver
{
private:
	static constexpr uint32_t TRANSCEIVER_ID = 0x00FFFFFF;

	struct HopRequestStruct
	{
		uint32_t StartTimestamp = 0;
		bool UpdatePending = false;

		const bool HasPending()
		{
			return UpdatePending;
		}

		void Request()
		{
			if (!UpdatePending)
			{
				StartTimestamp = micros();
				UpdatePending = true;
			}
		}

		void Clear()
		{
			UpdatePending = false;
		}
	};

	template<const uint8_t MaxPacketSize>
	struct IoPacketStruct
	{
		uint8_t Buffer[MaxPacketSize]{};
		uint32_t StartTimestamp = 0;
		uint8_t Size = 0;

		const bool HasPending()
		{
			return Size > 0;
		}

		virtual void Clear()
		{
			Size = 0;
			for (size_t i = 0; i < MaxPacketSize; i++)
			{
				Buffer[i] = 0;
			}
		}
	};

	template<const uint8_t MaxPacketSize>
	struct OutPacketStruct : public IoPacketStruct<MaxPacketSize>
	{
		uint8_t Channel = 0;
		bool AirStarted = false;
		virtual void Clear()
		{
			IoPacketStruct<MaxPacketSize>::Clear();
			Channel = 0;
			AirStarted = false;
		}
	};

	static constexpr uint8_t GetRawChannel(const uint8_t abstractChannel)
	{
		return GetRealChannel<Config::ChannelCount>(abstractChannel);
	}

private:
	ILoLaTransceiverListener* Listener = nullptr;

private:
	IVirtualTransceiver* Partner = nullptr;


private:
	IoPacketStruct<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE> Incoming{};
	OutPacketStruct<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE> OutGoing{};

	HopRequestStruct HopRequest{};

	uint8_t CurrentChannel = 0;

	bool DriverEnabled = false;

private:
	void PrintName()
	{
#ifdef DEBUG_LOLA
		Serial.print(millis());

		Serial.print('\t');
		Serial.print('[');
		Serial.print(OnwerName);
		Serial.print(']');
		Serial.print(' ');
#endif
	}

public:
	VirtualTransceiver(Scheduler& scheduler)
		: IVirtualTransceiver()
		, ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{}

public:
	virtual bool Callback() final
	{
		// Simulate transmit delay, from request to on-air start.
		if (OutGoing.HasPending() && ((micros() - OutGoing.StartTimestamp) >= GetTimeToAir(OutGoing.Size)))
		{
			if (CurrentChannel != OutGoing.Channel)
			{
				CurrentChannel = OutGoing.Channel;
				LogChannel(CurrentChannel);
			}

			if (!OutGoing.AirStarted)
			{
				if ((micros() - OutGoing.StartTimestamp) > GetTimeToAir(OutGoing.Size))
				{
					OutGoing.AirStarted = true;

#if defined(ECO_CHANCE)
					if (random(1 + UINT8_MAX) <= ECO_CHANCE)
					{
#if defined(DEBUG_LOLA)
						PrintName();
						Serial.println(F("Echo attack!"));
#endif
						ReceivePacket(OutGoing.Buffer, OutGoing.Size, CurrentChannel);
					}
#endif

#if defined(DOUBLE_SEND_CHANCE)
					if (random(1 + UINT8_MAX) <= DOUBLE_SEND_CHANCE)
					{
#if defined(DEBUG_LOLA)
						PrintName();
						Serial.println(F("Double send attack!"));
#endif
						Partner->ReceivePacket(OutGoing.Buffer, OutGoing.Size, CurrentChannel);
					}
#endif
					if (!Partner->ReceivePacket(OutGoing.Buffer, OutGoing.Size, OutGoing.Channel))
					{
#if defined(DEBUG_LOLA)
						PrintName();
						Serial.println(F("Tx Collision. Driver rejected."));
#endif
					}

#if defined(PRINT_PACKETS)
					PrintPacket(OutGoing.Buffer, OutGoing.Size);
#endif
					if (CurrentChannel != OutGoing.Channel)
					{
						CurrentChannel = OutGoing.Channel;
						LogChannel(CurrentChannel);
					}

					if (Listener != nullptr)
					{
						Listener->OnTx();
					}
				}
			}
			else if ((micros() - OutGoing.StartTimestamp) > GetDurationInAir(OutGoing.Size))
			{
				// After TX, TX channel is the current internal channel.
				Task::enable();
				OutGoing.Clear();
			}
		}

		// Simulate delay from start event to received packet buffer.
		if (Incoming.HasPending() && ((micros() - Incoming.StartTimestamp) >= GetDurationInAir(Incoming.Size)))
		{
			// Rx duration has elapsed since the packet incoming start triggered.
			if (Listener != nullptr)
			{
#if defined(DROP_CHANCE)
				if (random(1 + UINT8_MAX) <= DROP_CHANCE)
				{
#if defined(DEBUG_LOLA)
					PrintName();
					Serial.println(F("Drop attack!"));
#endif
				}
				else
#endif
					if (!Listener->OnRx(Incoming.Buffer, Incoming.StartTimestamp, Incoming.Size, UINT8_MAX / 2))
					{
#if defined(DEBUG_LOLA)
						PrintName();
						Serial.println(F("Rx Collision. Packet Service rejected."));
#endif
					}
			}
			Incoming.Clear();
		}

		if (OutGoing.HasPending())
		{
			Task::enable();

			return true;
		}

		if (HopRequest.HasPending())
		{
			if ((micros() - HopRequest.StartTimestamp) >= Config::HopMicros)
			{
				HopRequest.Clear();
			}
			Task::enable();

			return true;
		}
		else if (Incoming.HasPending())
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
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr;
	}

	virtual const bool Start() final
	{
		Incoming.Clear();
		OutGoing.Clear();
		HopRequest.Clear();

		CurrentChannel = 0;
		DriverEnabled = true;

		Task::enable();

		return true;
	}

	virtual const uint32_t GetTransceiverCode() final
	{
		return TRANSCEIVER_ID | ((uint32_t)Config::ChannelCount << 24);
	}

	virtual const bool Stop() final
	{
		DriverEnabled = false;

		Task::disable();

		return true;
	}

	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
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

		if (Incoming.HasPending() && ((micros() - Incoming.StartTimestamp) < GetDurationInAir(Incoming.Size)))
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Tx failed. Rx collision."));
#endif
			return false;
		}

		// Copy packet to temporary output buffer.
		// This will be distributed after TimeToAir,
		// simulating the transmit delay.
		OutGoing.Size = packetSize;
		OutGoing.StartTimestamp = timestamp;

		OutGoing.Channel = GetRawChannel(channel);
		for (uint_fast8_t i = 0; i < packetSize; i++)
		{
			OutGoing.Buffer[i] = data[i];
		}

		Task::enable();

		return true;
	}

	virtual void Rx(const uint8_t channel) final
	{
		const uint8_t rawChannel = GetRawChannel(channel);

		if (CurrentChannel != rawChannel)
		{
			HopRequest.Request();
			LogChannel(rawChannel);
		}

		CurrentChannel = rawChannel;

		Task::enable();
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return Config::TxBaseMicros + ((Config::TxByteNanos * packetSize) / 1000);
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		return Config::AirBaseMicros + ((Config::AirByteNanos * packetSize) / 1000);
	}

	virtual const bool TxAvailable() final
	{
		return DriverEnabled && !OutGoing.HasPending() && !Incoming.HasPending();
	}

public:
	virtual void SetPartner(IVirtualTransceiver* partner) final
	{
		Partner = partner;
	}

	virtual const bool ReceivePacket(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
	{
		const uint32_t timestamp = micros();

		if (CurrentChannel != channel)
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.print(F("Channel Miss: Rx:"));
			Serial.print(CurrentChannel);
			Serial.print(F(" Tx:"));
			Serial.println(channel);
#endif
			return false;
		}

		if (Incoming.HasPending())
		{
			if (Listener != nullptr)
			{
				Listener->OnRxLost(timestamp);
			}
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Virtual Rx Collision."));
#endif
			return false;
		}

		if (OutGoing.HasPending())
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Rx failed. Tx was pending."));
#endif
			return false;
		}

		if (HopRequest.HasPending() && ((timestamp - HopRequest.StartTimestamp) < Config::HopMicros))
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Rx failed. Rx was hopping."));
#endif
			return false;
		}

		Incoming.Size = packetSize;
		Incoming.StartTimestamp = timestamp;
		for (uint_fast8_t i = 0; i < packetSize; i++)
		{
			Incoming.Buffer[i] = data[i];
		}

#if defined(CORRUPT_CHANCE)
		if (random(1 + UINT8_MAX) <= CORRUPT_CHANCE)
		{
			Incoming.Buffer[random(packetSize)] = random(1 + UINT8_MAX);
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Corruption attack!"));
#endif
		}
#endif
		Task::enable();

		return true;
	}

private:
	void LogChannel(const uint8_t channel)
	{
		if (LogChannelHop)
		{
			LogChannel(channel, Config::ChannelCount, 1);
		}
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
		mac |= data[1] << 8;
		mac |= (uint32_t)data[2] << 16;
		mac |= (uint32_t)data[3] << 24;
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