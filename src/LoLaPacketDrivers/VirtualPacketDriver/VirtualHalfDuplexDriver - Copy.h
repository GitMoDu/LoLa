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
	const uint8_t PinTestChannelHop = 0,
	const bool LogChannelHop = false
>
class VirtualHalfDuplexDriver
	: private Task
	, public virtual IVirtualPacketDriver
	, public virtual ILoLaPacketDriver
{
private:
	static const uint32_t MaxPacketSize = LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE;

private:
	ILoLaPacketDriverListener* PacketListener = nullptr;

private:
	IVirtualPacketDriver* Partner = nullptr;

private:
	IVirtualPacketDriver::InPacketStruct<MaxPacketSize> OutPartner{};
	IVirtualPacketDriver::InPacketStruct<MaxPacketSize> Incoming{};
	//IVirtualPacketDriver::OutPacketStruct<MaxPacketSize> Outgoing{};


	struct TxTrackingStruct
	{
		uint32_t Free = 0;
		bool Pending = false;

		void Clear()
		{
			Pending = false;
		}
	} TxTracking{};

	//uint32_t TxFreeTimestamp = 0;
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
		if (PinTestChannelHop > 0)
		{
			digitalWrite(PinTestChannelHop, LOW);
			pinMode(PinTestChannelHop, OUTPUT);
		}
	}

public:
	virtual bool Callback() final
	{
		if (TxTracking.Pending && (micros() - TxTracking.Free > 0))
		{
			// Tx duration has elapsed, release send mode.
// This is where some radios automatically set back to RX.
			if (PacketListener != nullptr)
			{
				PacketListener->OnSent(true);
			}
			TxTracking.Pending = false;
			if (PinTestChannelHop > 0)
			{
				digitalWrite(PinTestChannelHop, LOW);
			}
		}

		if (OutPartner.HasPending() && (micros() - OutPartner.Started) > 0)
		{
			ProcessOutPartner();

			//				// Tx duration has elapsed, release send mode.
//				// This is where some radios automatically set back to RX.
//				if (PacketListener != nullptr)
//				{
//					PacketListener->OnSent(true);
//				}
//			}
		}

		//		if (Outgoing.HasPending())
		//		{
		//			// TODO: Post Tx delay?
		//			if (Outgoing.Delivered)
		//			{
		//				Outgoing.Clear();
		//
		//				// Tx duration has elapsed, release send mode.
		//				// This is where some radios automatically set back to RX.
		//				if (PacketListener != nullptr)
		//				{
		//					PacketListener->OnSent(true);
		//				}
		//			}
		//			else if ((micros() - Outgoing.Started) > 0)
		//				//else if ((micros() - Outgoing.Started) >= GetTransmitDurationMicros(Outgoing.Size))
		//			{
		//				if (PinTestChannelHop > 0)
		//				{
		//					digitalWrite(PinTestChannelHop, LOW);
		//				}
		//
		//				// Tx duration has elapsed, deliver packet.
		//				if (Partner != nullptr)
		//				{
		//					Partner->ReceivePacket(Outgoing.Buffer, Outgoing.Size, CurrentChannel);
		//				}
		//				Outgoing.Delivered = true;
		//
		//#if defined(DOUBLE_SEND_CHANCE)
		//				if (random(UINT8_MAX) < DOUBLE_SEND_CHANCE)
		//				{
		//#if defined(DEBUG_LOLA)
		//					PrintName();
		//					Serial.println(F("Double send attack!"));
		//#endif
		//					Partner->ReceivePacket(Outgoing.Buffer, Outgoing.Size, CurrentChannel);
		//			}
		//#endif
		//
		//#if defined(ECO_CHANCE)
		//				if (random(UINT8_MAX) < ECO_CHANCE)
		//				{
		//					Incoming.Started = micros();
		//					Incoming.Size = Outgoing.Size;
		//					for (uint8_t i = 0; i < Outgoing.Size; i++)
		//					{
		//						Incoming.Buffer[i] = Outgoing.Buffer[i];
		//					}
		//
		//					Task::enableIfNot();
		//#if defined(DEBUG_LOLA)
		//					PrintName();
		//					Serial.println(F("Eco attack!"));
		//#endif
		//		}
		//#endif
		//	}
		//}

		if (Incoming.HasPending())
		{
			if (micros() - Incoming.Started > GetRxDuration(Incoming.Size))
			{
				// Rx duration has elapsed since the packet incoming start triggered.
				if (PacketListener != nullptr)
				{
					PacketListener->OnReceived(Incoming.Buffer, Incoming.Started, Incoming.Size, UINT8_MAX / 2);
				}

				Incoming.Clear();
			}
		}

		//if (Outgoing.HasPending() || Incoming.HasPending())
		if (OutPartner.HasPending() || Incoming.HasPending())
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
		//Outgoing.Delivered = false;
		TxTracking.Clear();

		CurrentChannel = 0;
		DriverEnabled = true;
		//TxFreeTimestamp = micros();

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

		if (TxTracking.Pending)
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Tx failed. Was sending."));
#endif

			return false;
		}

#if defined(DEBUG_LOLA)
		if (packetSize < LoLaPacketDefinition::MIN_PACKET_SIZE || packetSize > MaxPacketSize)
		{
			PrintName();
			Serial.print(F("Transmit failed. Invalid packet Size ("));
			Serial.print(packetSize);
			Serial.println(F(") bytes."));

			return false;
		}
#endif
		if ((CurrentChannel != channel % Config::ChannelCount))
		{
			CurrentChannel = channel % Config::ChannelCount;
			if (LogChannelHop)
			{
				LogChannel();
			}
		}


		if (PinTestChannelHop > 0)
		{
			digitalWrite(PinTestChannelHop, HIGH);
		}

		TxTracking.Pending = true;
		TxTracking.Free = timestamp + GetTransmitDurationMicros(packetSize);

		Partner->ReceivePacket(data, packetSize, CurrentChannel);



		//		if (!Outgoing.HasPending())
		//		{
		//			Outgoing.Started = timestamp;
		//			Outgoing.Size = packetSize;
		//			Outgoing.Delivered = false;
		//
		//			for (size_t i = 0; i < packetSize; i++)
		//			{
		//				Outgoing.Buffer[i] = data[i];
		//			}
		//
		//			if ((CurrentChannel != channel % Config::ChannelCount))
		//			{
		//				CurrentChannel = channel % Config::ChannelCount;
		//				if (LogChannelHop)
		//				{
		//					LogChannel();
		//				}
		//			}
		//
		//			Task::enable();
		//
		//			if (PinTestChannelHop > 0)
		//			{
		//				digitalWrite(PinTestChannelHop, HIGH);
		//			}
		//
		//			//#if defined(DEBUG_LOLA)
		//			//			Serial.print(millis());
		//			//			Serial.print('\t');
		//			//			PrintName();
		//			//			Serial.println(F("Virtual Tx."));
		//			//#endif
		//		}
	//		else
	//		{
	//#if defined(DEBUG_LOLA)
	//			PrintName();
	//			Serial.println(F("Tx failed. Was sending."));
	//#endif
	//		}

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

		//TxFreeTimestamp = micros();
	}

	virtual const uint32_t GetTransmitDurationMicros(const uint8_t packetSize) final
	{
		return Config::TxBase + ((Config::TxByteNanos * packetSize) / 1000);
	}

	virtual const uint32_t GetHopDurationMicros() final
	{
		return Config::HopMicros;
	}

	virtual const bool DriverCanTransmit() final
	{
		return DriverEnabled && !Incoming.HasPending() && !TxTracking.Pending;
		//&& !Outgoing.HasPending();
	}

public:
	virtual void SetPartner(IVirtualPacketDriver* partner) final
	{
		Partner = partner;
	}


	virtual void ReceivePacket(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
	{
		// Copy packet to temporary local copy of partner output.
		// This will be distributed as Incoming after TransmitDuration,
		// simulating the transmit delay without async calls.
		OutPartner.Started = micros() + GetTransmitDurationMicros(packetSize);
		OutPartner.Size = packetSize;
		OutPartner.Channel = channel;
		for (uint_fast8_t i = 0; i < packetSize; i++)
		{
			OutPartner.Buffer[i] = data[i];
		}
	}

	void ProcessOutPartner()
	{
		//}

		//virtual void ReceivePacket(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
		//{
		//const uint32_t timestamp = micros();

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
			Task::enable();
			return;
		}

		//if (Outgoing.HasPending() && !Outgoing.Delivered)
		//{
		//	if (PacketListener != nullptr)
		//	{
		//		PacketListener->OnReceiveLost(timestamp);
		//	}
		//	//#if defined(DEBUG_LOLA)
		//	//			Serial.print(millis());
		//	//			Serial.print('\t');
		//	//			PrintName();
		//	//			Serial.println(F("Rx Collision. Was sending."));
		//	//#endif
		//	return;
		//}

		if (Incoming.HasPending())
		{
			//if (PacketListener != nullptr)
			//{
			//	PacketListener->OnReceiveLost(timestamp);
			//}
#if defined(DEBUG_LOLA)
			Serial.print(millis());
			Serial.print('\t');
			PrintName();
			Serial.println(F("Rx Collision. Was processing."));
#endif
			return;
		}

#if defined(DROP_CHANCE)
		if (random(UINT8_MAX) < DROP_CHANCE)
		{
#if defined(DEBUG_LOLA)
			PrintName();
			Serial.println(F("Packet drop attack!"));
#endif
			if (PacketListener != nullptr)
			{
				PacketListener->OnReceiveLost(timestamp);
			}

			Task::enable();
			return;
		}
#endif

		//if ((CurrentChannel != channel % Config::ChannelCount))
		//{
		//	CurrentChannel = channel % Config::ChannelCount;
		//	if (LogChannelHop)
		//	{
		//		LogChannel();
		//	}
		//}

		Incoming.Size = OutPartner.Size;
		Incoming.Started = OutPartner.Started;
		for (uint_fast8_t i = 0; i < OutPartner.Size; i++)
		{
			Incoming.Buffer[i] = OutPartner.Buffer[i];
		}


		Task::enable();
		//Incoming.Size = packetSize;
		//Incoming.Started = timestamp;
		//for (uint_fast8_t i = 0; i < packetSize; i++)
		//{
		//	Incoming.Buffer[i] = data[i];
		//}

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
	void LogChannel()
	{
#ifdef DEBUG_LOLA
		IVirtualPacketDriver::LogChannel(CurrentChannel, Config::ChannelCount, 1);
#endif
	}
};
#endif