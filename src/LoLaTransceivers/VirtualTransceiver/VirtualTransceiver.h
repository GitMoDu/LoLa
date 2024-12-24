// VirtualTransceiver.h

#ifndef _VIRTUAL_TRANSCEIVER_h
#define _VIRTUAL_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include "../ILoLaTransceiver.h"
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
	const bool LogChannelHop = false>
class VirtualTransceiver final
	: private TS::Task
	, public virtual IVirtualTransceiver
	, public virtual ILoLaTransceiver
{
private:
	static constexpr uint32_t TRANSCEIVER_ID = 0x00FFFFFF;

	static constexpr uint32_t PAUSE_AFTER_TX_MICROS = 20 + Config::HopMicros;

	static constexpr uint8_t RX_RSSI_MIN = UINT8_MAX / 3;

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

	uint32_t LastOut = 0;

	uint8_t CurrentChannel = 0;

	bool DriverEnabled = false;

private:
	void PrintName()
	{
#ifdef DEBUG_LOLA_LINK
		Serial.print(millis());

		Serial.print('\t');
		Serial.print('[');
		Serial.print(OnwerName);
		Serial.print(']');
		Serial.print(' ');
#endif
	}

	static const uint8_t GetRxRssi()
	{
		return RX_RSSI_MIN + random(UINT8_MAX - RX_RSSI_MIN + 1);
	}

public:
	VirtualTransceiver(TS::Scheduler& scheduler)
		: IVirtualTransceiver()
		, ILoLaTransceiver()
		, TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
	}


public:
	bool Callback() final
	{
		// Simulate transmit delay, from request to on-air start.
		if (OutGoing.HasPending())
		{
			if (CurrentChannel != OutGoing.Channel)
			{
				CurrentChannel = OutGoing.Channel;
				LogChannel(CurrentChannel);
			}

			if (((micros() - OutGoing.StartTimestamp) >= GetTimeToAir(OutGoing.Size)))
			{
				if (!OutGoing.AirStarted)
				{
					OutGoing.AirStarted = true;

#if defined(ECO_CHANCE)
					if (RollDice(ECO_CHANCE))
					{
#if defined(DEBUG_LOLA_LINK)
						PrintName();
						Serial.println(F("Echo attack!"));
#endif
						ReceivePacket(OutGoing.Buffer, micros(), OutGoing.Size, CurrentChannel);
					}
#endif

#if defined(DOUBLE_SEND_CHANCE)
					if (RollDice(DOUBLE_SEND_CHANCE))
					{
#if defined(DEBUG_LOLA_LINK)
						PrintName();
						Serial.println(F("Double send attack!"));
#endif
						Partner->ReceivePacket(OutGoing.Buffer, micros(), OutGoing.Size, CurrentChannel);
					}
#endif
					Partner->ReceivePacket(OutGoing.Buffer, OutGoing.StartTimestamp, OutGoing.Size, OutGoing.Channel);
#if defined(PRINT_PACKETS)
					PrintPacket(OutGoing.Buffer, OutGoing.Size);
#endif
				}
				else if ((micros() - OutGoing.StartTimestamp) > GetTimeToAir(OutGoing.Size) + GetDurationInAir(OutGoing.Size))
				{
					// After TX, TX channel is the current internal channel.
					if (Listener != nullptr)
					{
						Listener->OnTx();
					}
					TS::Task::enable();
					OutGoing.Clear();
					LastOut = micros();
				}
			}
		}

		// Simulate delay from start event to received packet buffer.
		else if (Incoming.HasPending() && ((micros() - Incoming.StartTimestamp) >= GetDurationInAir(Incoming.Size)))
		{
			// Rx duration has elapsed since the packet incoming start triggered.
			if (Listener != nullptr)
			{
#if defined(DROP_CHANCE)
				if (RollDice(DROP_CHANCE))
				{
#if defined(DEBUG_LOLA_LINK)
					PrintName();
					Serial.println(F("Drop attack!"));
#endif
				}
				else
#endif
					if (!Listener->OnRx(Incoming.Buffer, Incoming.StartTimestamp, Incoming.Size, GetRxRssi()))
					{
#if defined(DEBUG_LOLA_LINK)
						PrintName();
						Serial.println(F("Rx Collision. Packet rejected."));
#endif
					}
			}
			Incoming.Clear();
		}

		if (OutGoing.HasPending())
		{
			TS::Task::enable();

			return true;
		}

		if (HopRequest.HasPending())
		{
			if ((micros() - HopRequest.StartTimestamp) >= Config::HopMicros)
			{
				HopRequest.Clear();
			}
			TS::Task::enable();

			return true;
		}
		else if (Incoming.HasPending())
		{
			TS::Task::enable();

			return true;
		}
		else
		{
			TS::Task::disable();
			return false;
		}
	}

public:
	const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr;
	}

	const bool Start() final
	{
		Incoming.Clear();
		OutGoing.Clear();
		HopRequest.Clear();

		LastOut = micros() - PAUSE_AFTER_TX_MICROS;
		CurrentChannel = 0;
		DriverEnabled = true;

		TS::Task::enable();

		return true;
	}

	const uint32_t GetTransceiverCode() final
	{
		return TRANSCEIVER_ID | ((uint32_t)Config::ChannelCount << 24);
	}

	const bool Stop() final
	{
		DriverEnabled = false;

		TS::Task::disable();

		return true;
	}

	const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
	{
		const uint32_t timestamp = micros();

		if (!TxAvailable())
		{
#if defined(DEBUG_LOLA_LINK)
			PrintName();
			Serial.println(F("Tx failed. Tx not available."));
#endif
			return false;
		}

		if (packetSize < LoLaPacketDefinition::MIN_PACKET_SIZE || packetSize > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
		{
#if defined(DEBUG_LOLA_LINK)
			PrintName();
			Serial.print(F("Tx failed. Invalid packet Size ("));
			Serial.print(packetSize);
			Serial.println(F(") bytes."));
#endif
			return false;
		}

		if (OutGoing.HasPending())
		{
#if defined(DEBUG_LOLA_LINK)
			PrintName();
			Serial.println(F("Tx failed. Was sending."));
#endif
			return false;
		}

		if (Incoming.HasPending() && ((micros() - Incoming.StartTimestamp) < GetDurationInAir(Incoming.Size)))
		{
#if defined(DEBUG_LOLA_LINK)
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
		memcpy(OutGoing.Buffer, data, packetSize);

		TS::Task::enable();

		return true;
	}

	void Rx(const uint8_t channel) final
	{
		const uint8_t rawChannel = GetRawChannel(channel);

		if (CurrentChannel != rawChannel)
		{
			HopRequest.Request();
			CurrentChannel = rawChannel;
			LogChannel(rawChannel);
		}

		TS::Task::enable();
	}

	const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return Config::TxBaseMicros + (((uint32_t)Config::TxByteNanos * packetSize) / 1000);
	}

	const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		return Config::AirBaseMicros + (((uint32_t)Config::AirByteNanos * packetSize) / 1000);
	}

	const bool TxAvailable() final
	{
		return DriverEnabled && !OutGoing.HasPending() && !Incoming.HasPending() && (micros() - LastOut >= PAUSE_AFTER_TX_MICROS);
	}

	const uint8_t GetChannelCount() final
	{
		return Config::ChannelCount;
	}

	const uint8_t GetCurrentChannel() final
	{
		return CurrentChannel;
	}

public:
	void SetPartner(IVirtualTransceiver* partner) final
	{
		Partner = partner;
	}

	void ReceivePacket(const uint8_t* data, const uint32_t txTimestamp, const uint8_t packetSize, const uint8_t channel) final
	{
		if (HopRequest.HasPending() && ((micros() - HopRequest.StartTimestamp) < Config::HopMicros))
		{
#if defined(DEBUG_LOLA_LINK)
			PrintName();
			Serial.println(F("Rx failed. Rx was hopping."));
#endif
			return;
		}

		if (Incoming.HasPending())
		{
#if defined(DEBUG_LOLA_LINK)
			PrintName();
			Serial.println(F("Rx Collision, Rx was pending."));
#endif
			return;
		}

		if (OutGoing.HasPending())
		{
#if defined(DEBUG_LOLA_LINK)
			PrintName();
			Serial.println(F("Rx failed. Tx was pending."));
#endif
			return;
		}

		if (CurrentChannel != channel)
		{
#if defined(DEBUG_LOLA_LINK)
			PrintName();
			Serial.print(F("Channel Miss: Rx:"));
			Serial.print(CurrentChannel);
			Serial.print(F(" Tx:"));
			Serial.println(channel);
#endif
			return;
		}

		Incoming.Size = packetSize;
		Incoming.StartTimestamp = txTimestamp;
		memcpy(Incoming.Buffer, data, packetSize);

#if defined(CORRUPT_CHANCE)
		if (random(1 + UINT8_MAX) <= CORRUPT_CHANCE)
		{
			Incoming.Buffer[random(packetSize)] = random(1 + UINT8_MAX);
#if defined(DEBUG_LOLA_LINK)
			PrintName();
			Serial.println(F("Corruption attack!"));
#endif
		}
#endif
		TS::Task::enable();
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
#endif

	const bool RollDice(const uint8_t chance)
	{
		return random(200) <= (long)chance;
	}
};
#endif