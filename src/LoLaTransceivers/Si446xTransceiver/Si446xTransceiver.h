// Si446xTransceiver.h

#ifndef _LOLA_SI446X_TRANSCEIVER_h
#define _LOLA_SI446X_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>


#include <Si446x.h>
#if SI446X_INTERRUPTS != 0
error("Must set SI446X_INTERRUPTS = 0 in Si446x_config.h")
#endif

#include "Support\Si4463Events.h"
#include "Support\Si4463Config.h"

/*
* https://github.com/ZakKemble/Si446x
*/
#include <Si446x.h>
#include "radio_config.h"

/// <summary>
/// 
/// </summary>
/// <typeparam name="CsPin"></typeparam>
/// <typeparam name="SdnPin"></typeparam>
/// <typeparam name="InterruptPin"></typeparam>
template<const uint8_t CsPin,
	const uint8_t SdnPin,
	const uint8_t InterruptPin>
class Si446xTransceiver final
	: private Task, public virtual ILoLaTransceiver
{
	static constexpr uint8_t ChannelCount = Si4463Config::GetRadioConfigChannelCount(MODEM_2_1);

	static constexpr uint16_t TRANSCEIVER_ID = 0x4463;

private:
	// TODO: These values might change with different SPI and CPU clock speed.
	static constexpr uint16_t TTA_LONG = 719;
	static constexpr uint16_t TTA_SHORT = 638;

	static constexpr uint16_t DIA_LONG = 1923;
	static constexpr uint16_t DIA_SHORT = 987;

private:
	using ErrorEnum = Si4463Events::ErrorEnum;

private:
	void (*RadioInterrupt)(void) = nullptr;

	ILoLaTransceiverListener* Listener = nullptr;

private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	Si4463Events::InterruptEventStruct RadioEvent{};

	Si4463Events::TxEventStruct TxEvent{};
	Si4463Events::RxEventStruct RxEvent{};
	Si4463Events::RxTriggerStruct RxTrigger{};

	Si4463Events::RxStruct RxPending{};
	Si4463Events::TxStruct TxPending{};

	Si4463Events::HopStruct HopPending{};

public:
	Si446xTransceiver(Scheduler& scheduler)
		: ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
		pinMode(CsPin, INPUT);
		pinMode(SdnPin, INPUT);
		pinMode(InterruptPin, INPUT);
	}

	void SetupInterrupt(void (*onRadioInterrupt)(void))
	{
		RadioInterrupt = onRadioInterrupt;
		DisableInterrupt();
		pinMode(InterruptPin, INPUT);
	}

	void OnRadioInterrupt()
	{
		DisableInterrupt();
		RadioEvent.Pending = true;
		RadioEvent.Timestamp = micros();
		Task::enable();
	}

	void OnTxInterrupt()
	{
		if (TxEvent.Pending)
		{
			TxEvent.Collision = true;
		}
		else
		{
			TxEvent.Pending = true;
		}
		Task::enable();
	}

	void OnPreRxInterrupt()
	{
#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, HIGH);
#endif
		if (RxTrigger.Pending)
		{
			RxTrigger.Collision = true;
		}
		else
		{
			RxTrigger.Pending = true;
			RxTrigger.StartTimestamp = RadioEvent.Timestamp;
		}
		Task::enable();
	}

	void OnPostRxInterrupt(const uint8_t size, const int16_t rssi)
	{
		if (RxEvent.Pending())
		{
			RxEvent.Collision = true;
		}
		else
		{
			RxEvent.Size = size;
			RxEvent.Rssi = rssi;
		}
		Task::enable();
	}

public:
	// ILoLaTransceiver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr;
	}

	virtual const bool Start() final
	{
		if (Listener != nullptr
			&& Si4463Config::ValidateConfig(MODEM_2_1))
		{
			Si446x_init();

			// Enable packet RX begin and packet sent callbacks
			Si446x_setupCallback(
				SI446X_CBS_SENT
				| SI446X_CBS_RXBEGIN
				| SI446X_CBS_RXCOMPLETE
				, 1);

			Si446x_setTxPower(5);

			// Sleep until first command.
			Si446x_sleep();

			// Start listening to IRQ.
			pinMode(InterruptPin, INPUT_PULLUP);
			EnableInterrupt();
			Task::enable();

			return true;
		}

		return false;
	}

	virtual const bool Stop() final
	{
		DisableInterrupt();
		pinMode(InterruptPin, INPUT);
		Task::disable();

		return true;
	}

	virtual const bool TxAvailable() final
	{
		return !HopPending.Pending()
			&& !RxEvent.Pending()
			&& !RxTrigger.Pending
			&& !RadioEvent.Pending
			&& !TxPending.Pending
			&& !TxEvent.Pending
			&& Si446x_getState() == si446x_state_t::SI446X_STATE_RX;
	}

	virtual const uint32_t GetTransceiverCode()
	{
		return (uint32_t)TRANSCEIVER_ID
			| (uint32_t)Si4463Config::GetConfigId(MODEM_2_1) << 16
			| (uint32_t)ChannelCount << 24;
	}

	/// <summary>
	/// Packet copy to serial buffer, and start transmission.
	/// The first byte is the packet size, excluding the size byte.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="length"></param>
	/// <returns>True if transmission was successful.</returns> 
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		const uint8_t rawChannel = GetRawChannel(channel);

		// Store Tx channel as last channel.
		HopPending.Channel = rawChannel;
		Task::enable();

		if (1 == Si446x_TX((uint8_t*)data, packetSize, rawChannel, si446x_state_t::SI446X_STATE_RX))
		{
			TxPending.Pending = true;
			TxPending.StartTimestamp = micros();

			return true;
		}
		else
		{
			HopPending.Request();
		}

		return false;
	}

	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		const uint8_t rawChannel = GetRawChannel(channel);
		HopPending.Request(rawChannel);
		Task::enable();
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		if (packetSize <= LoLaPacketDefinition::MIN_PACKET_SIZE)
		{
			return TTA_SHORT;
		}
		else
		{
			return TTA_SHORT + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * (TTA_LONG - TTA_SHORT)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
		}
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		if (packetSize <= LoLaPacketDefinition::MIN_PACKET_SIZE)
		{
			return DIA_SHORT;
		}
		else
		{
			return DIA_SHORT + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * (DIA_LONG - DIA_SHORT)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
		}
	}

public:
	virtual bool Callback() final
	{
		if (RadioEvent.Pending)
		{
			// Clear the event only after servicing.
			// consumes RadioEvent timestamp.
			Si446x_SERVICE();
			RadioEvent.Clear();
			EnableInterrupt();

			Task::enable();
			return true;
		}

		uint16_t error = 0;
		if (TxEvent.Pending)
		{
			if (TxPending.Pending)
			{
				Listener->OnTx();

				if (TxPending.Collision)
				{
					error |= (uint16_t)ErrorEnum::TxCollision;
				}
				TxPending.Clear();
			}
			else
			{
				error |= (uint16_t)ErrorEnum::TxUnexpected;
			}

			TxEvent.Clear();
			HopPending.Request();
		}

		// Check for TX timeout.
		if (TxPending.Pending
			&& (micros() - TxPending.StartTimestamp > Si4463Config::TX_TIMEOUT_MICROS))
		{
			TxPending.Clear();
			HopPending.Request();
			Listener->OnTx();
			error |= (uint16_t)ErrorEnum::TxTimeout;
		}

		// Distribute any pending packets before responding to RxEvent.
		if (RxPending.Pending())
		{
			Listener->OnRx(InBuffer, RxPending.StartTimestamp, RxPending.Size, RxPending.Rssi);
			RxPending.Clear();
		}

		// Check for Rx timeout, after OnPreRxInterrupt.
		if (RxTrigger.Pending
			&& (micros() - RxTrigger.StartTimestamp > Si4463Config::RX_TIMEOUT_MICROS))
		{
			// Clear failed Rx.
			RxTrigger.Clear();
			HopPending.Request();
		}

		if (RxEvent.Pending())
		{
			if (RxTrigger.Pending)
			{
				if (RxTrigger.Collision)
				{
					error |= (uint16_t)ErrorEnum::RxTriggerCollision;
				}

				RxTrigger.Clear();

				if (RxEvent.Size >= LoLaPacketDefinition::MIN_PACKET_SIZE
					&& RxEvent.Size <= LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
				{
					Si446x_read(InBuffer, RxEvent.Size);
					// Reading the data over SPI already took some time.
					// The buffered packet will be distributed on next run instead.
					// Since we have to double buffer the input, might as well use it to avoid skipping packets.
					RxPending.Size = RxEvent.Size;
					RxPending.Rssi = GetNormalizedRssi(RxEvent.Rssi);
					RxPending.StartTimestamp = RxTrigger.StartTimestamp;
				}
				else
				{
					// Error triggers hop. Hop clears Rx FIFO.
					error |= (uint16_t)ErrorEnum::RxInvalidSize;
				}
			}
			else
			{
				error |= (uint16_t)ErrorEnum::RxOrphan;
				Listener->OnRxLost(RxTrigger.StartTimestamp);
			}

			if (RxEvent.Collision)
			{
				error |= (uint16_t)ErrorEnum::RxPacketCollision;
			}
			RxEvent.Clear();
		}

		// Recover from possible event collisions.
		if (error > 0)
		{
			Si446x_SERVICE();
			Si446x_sleep();
			HopPending.Request();
		}

		// Hop if it is time and other transactions aren't occurring.
		if (HopPending.Pending()
			&& !RadioEvent.Pending
			&& !RxTrigger.Pending
			&& !TxPending.Pending
			&& !RxEvent.Pending())
		{
			if (HopPending.Requested)
			{
				HopPending.Requested = false;
				HopPending.Resting = true;
				Si446x_RX(HopPending.Channel);
				HopPending.StartTimestamp = micros();
			}
			else if (micros() - HopPending.StartTimestamp > Si4463Config::HOP_DEFAULT)
			{
				HopPending.Clear();
			}
		}

#if defined(DEBUG_LOLA)
		if (error > 0)
		{
			Serial.println(F("Si446x Recovered from error:"));
			LogError(error);
		}
#endif

#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, LOW);
#endif

		if (RadioEvent.Pending
			|| TxEvent.Pending
			|| TxPending.Pending
			|| RxTrigger.Pending
			|| RxEvent.Pending()
			|| RxPending.Pending()
			|| HopPending.Pending())
		{
			Task::enable();
		}
		else
		{
			Task::disable();
		}

		return true;
	}

private:
	/// </summary>
	/// Converts the LoLa abstract channel into a real channel index for the radio.
	/// </summary>
	/// <param name="abstractChannel">LoLa abstract channel [0;255].</param>
	/// <returns>Returns the real channel to use [0;(ChannelCount-1)].</returns>
	static constexpr uint8_t GetRawChannel(const uint8_t abstractChannel)
	{
		return GetRealChannel<ChannelCount>(abstractChannel);
	}

	const uint8_t GetRealPower(const uint8_t normalizedPower)
	{
		return ((uint16_t)normalizedPower * 20) / UINT8_MAX;
	}

	const uint8_t GetNormalizedRssi(const int16_t rssi)
	{
		return ((150 - rssi) * UINT8_MAX) / 90;
	}

	void EnableInterrupt()
	{
		attachInterrupt(digitalPinToInterrupt(InterruptPin), RadioInterrupt, FALLING);
	}

	void DisableInterrupt()
	{
		detachInterrupt(digitalPinToInterrupt(InterruptPin));
	}

#if defined(DEBUG_LOLA)
	void LogError(const uint16_t error)
	{
		if (error & (uint16_t)ErrorEnum::TxCollision)
		{
			Serial.println(F("\tTx Collision."));
		}
		if (error & (uint16_t)ErrorEnum::TxUnexpected)
		{
			Serial.println(F("\tTx Unexpected."));
		}
		if (error & (uint16_t)ErrorEnum::TxTimeout)
		{
			Serial.println(F("\tTx timeout."));
		}
		if (error & (uint16_t)ErrorEnum::RxOrphan)
		{
			Serial.println(F("\tRx Orphan Packet."));
		}
		if (error & (uint16_t)ErrorEnum::RxTriggerCollision)
		{
			Serial.println(F("\tRx Trigger Collision."));
		}
		if (error & (uint16_t)ErrorEnum::RxPacketCollision)
		{
			Serial.println(F("\tRx PacketCollision."));
		}
		if (error & (uint16_t)ErrorEnum::RxInvalidSize)
		{
			Serial.println(F("\tRx RxInvalid Size."));
		}
	}
#endif
};
#endif