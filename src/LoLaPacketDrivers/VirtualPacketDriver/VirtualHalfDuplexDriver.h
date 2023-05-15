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
	const uint8_t PinTestTx = 0
>
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

		void Clear()
		{
			Size = 0;
		}
	};

	template<const uint8_t MaxPacketSize>
	struct InPacketStruct : public IoPacketStruct<MaxPacketSize> {};

	template<const uint8_t MaxPacketSize>
	struct DelayPacketStruct : IoPacketStruct<MaxPacketSize>
	{
		uint8_t Channel = 0;
	};

	struct TxTrackingStruct
	{
		uint32_t Started = 0;
		uint8_t Size = 0;
		uint8_t Channel = 0;

		const bool HasPending()
		{
			return Size > 0;
		}

		void SetSize(const uint8_t size)
		{
			if (PinTestTx > 0 && size > 0)
			{
				digitalWrite(PinTestTx, HIGH);
			}
			if (PinTestTx > 0)
			{
				digitalWrite(PinTestTx, LOW);
			}
			Size = size;
		}

		void Clear()
		{
			if (PinTestTx > 0)
			{
				digitalWrite(PinTestTx, LOW);
			}
			Size = 0;
		}

		TxTrackingStruct()
		{
			if (PinTestTx > 0)
			{
				digitalWrite(PinTestTx, LOW);
				pinMode(PinTestTx, OUTPUT);
			}
		}
	};

private:
	ILoLaPacketDriverListener* PacketListener = nullptr;

private:
	IVirtualPacketDriver* Partner = nullptr;


private:
	DelayPacketStruct<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE> OutPartner{};
	InPacketStruct<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE> Incoming{};

	TxTrackingStruct TxTracking{};

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
		if (OutPartner.HasPending() && ((micros() - OutPartner.Started) > GetTransmitDurationMicros(OutPartner.Size)))
		{
			ProcessOutPartner();
		}

		if (TxTracking.HasPending() && ((micros() - TxTracking.Started) > GetTransmitDurationMicros(TxTracking.Size)))
		{
			TxTracking.Clear();

			if ((CurrentChannel != TxTracking.Channel % Config::ChannelCount))
			{
				CurrentChannel = TxTracking.Channel % Config::ChannelCount;
				if (LogChannelHop)
				{
					LogChannel();
				}
			}



			// Tx duration has elapsed, release send mode.
			// This is where some radios automatically set back to RX.
			if (PacketListener != nullptr)
			{
				PacketListener->OnSent(true);
			}
		}

		if (Incoming.HasPending())
		{
			if (micros() - Incoming.Started > GetRxDuration(Incoming.Size))
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
		}

		if (TxTracking.HasPending() || OutPartner.HasPending() || Incoming.HasPending())
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
		OutPartner.Clear();
		TxTracking.Clear();

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

		if (packetSize < LoLaPacketDefinition::MIN_PACKET_SIZE || packetSize > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.print(F("Transmit failed. Invalid packet Size ("));
			Serial.print(packetSize);
			Serial.println(F(") bytes."));
#endif
			return false;
		}

		if (TxTracking.HasPending())
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Tx failed. Was sending."));
#endif
			return false;
		}


		TxTracking.SetSize(packetSize);
		TxTracking.Channel = channel % Config::ChannelCount;
		TxTracking.Started = timestamp;
		Task::enable();

		Partner->ReceivePacket(data, packetSize, channel % Config::ChannelCount);

		return true;
	}

	virtual void DriverRx(const uint8_t channel) final
	{
		if ((CurrentChannel != channel % Config::ChannelCount))
		{
			CurrentChannel = channel % Config::ChannelCount;
			if (LogChannelHop)
			{
				LogChannel();
			}
		}
	}

	virtual const uint32_t GetTransmitDurationMicros(const uint8_t packetSize) final
	{
		return Config::TxBase + ((Config::TxByteNanos * packetSize) / 1000);
	}

	virtual const bool DriverCanTransmit() final
	{
		return DriverEnabled && !Incoming.HasPending() && !TxTracking.HasPending();
	}

public:
	virtual void SetPartner(IVirtualPacketDriver* partner) final
	{
		Partner = partner;
	}


	virtual void ReceivePacket(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
	{
		const uint32_t timestamp = micros();

		if (OutPartner.HasPending())
		{
			if (PacketListener != nullptr)
			{
				PacketListener->OnReceiveLost(timestamp);
			}
#if defined(DEBUG_LOLA)
			Serial.print(millis());
			Serial.print('\t');
			PrintName();
			Serial.println(F("Rx Collision. Was already receiving."));
#endif
			return;
		}

		// Copy packet to temporary local copy of partner output.
		// This will be distributed as Incoming after TransmitDuration,
		// simulating the transmit delay without async calls.
		OutPartner.Started = timestamp;
		OutPartner.Size = packetSize;
		OutPartner.Channel = channel;
		for (uint_fast8_t i = 0; i < packetSize; i++)
		{
			OutPartner.Buffer[i] = data[i];
		}

		Task::enable();
	}

	void ProcessOutPartner()
	{
		if (CurrentChannel != OutPartner.Channel)
		{
#if defined(DEBUG_LOLA)
			Serial.print(millis());
			Serial.print('\t');
			PrintName();
			Serial.print(F("Channel Miss: Rx:"));
			Serial.print(CurrentChannel);
			Serial.print(F(" Tx:"));
			Serial.println(OutPartner.Channel);
#endif
			return;
		}

		if (TxTracking.HasPending())
		{
			if (PacketListener != nullptr)
			{
				PacketListener->OnReceiveLost(OutPartner.Started + GetTransmitDurationMicros(OutPartner.Size));
			}
#if defined(DEBUG_LOLA)
			Serial.print(millis());
			Serial.print('\t');
			PrintName();
			Serial.println(F("Rx Collision. Was Sending."));
#endif
			return;
		}

		if (Incoming.HasPending())
		{
			if (PacketListener != nullptr)
			{
				PacketListener->OnReceiveLost(OutPartner.Started + GetTransmitDurationMicros(OutPartner.Size));
			}
#if defined(DEBUG_LOLA)
			Serial.print(millis());
			Serial.print('\t');
			PrintName();
			Serial.println(F("Rx Collision. Was processing."));
#endif
			return;
		}

		Incoming.Size = OutPartner.Size;
		Incoming.Started = OutPartner.Started + GetTransmitDurationMicros(OutPartner.Size);
		for (uint_fast8_t i = 0; i < OutPartner.Size; i++)
		{
			Incoming.Buffer[i] = OutPartner.Buffer[i];
		}

		OutPartner.Clear();
		Task::enable();
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
	}

	const uint32_t GetRxDuration(const uint8_t size)
	{
		return Config::RxBase + ((Config::RxByteNanos * size)) / 1000;
	}

private:
	void LogChannel()
	{
#ifdef DEBUG_LOLA
		LogChannel(CurrentChannel, Config::ChannelCount, 1);
#endif
	}

#if defined(DEBUG_LOLA)
	void LogChannel(const uint8_t channel, const uint8_t channelCount, const uint8_t channelDivisor)
	{
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
	}
#endif

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