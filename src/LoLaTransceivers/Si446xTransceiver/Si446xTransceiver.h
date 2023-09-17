// Si446xTransceiver.h

#ifndef _LOLA_SI446X_TRANSCEIVER_h
#define _LOLA_SI446X_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>

#include <Si446x.h>
#include "Si4463Support.h"



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
	static constexpr uint8_t ChannelCount = LoLaSi4463Config::GetRadioConfigChannelCount(MODEM_2_1);

	static constexpr uint16_t TRANSCEIVER_ID = 0x4463;

private:
	// TODO: These values might change with different SPI and CPU clock speed.
	static constexpr uint16_t TTA_LONG = 719;
	static constexpr uint16_t TTA_SHORT = 638;

	static constexpr uint16_t DIA_LONG = 1923;
	static constexpr uint16_t DIA_SHORT = 987;

private:
	// TODO: Only InterruptEvent should need volatile members.
	struct InterruptEventStruct
	{
		volatile uint32_t Timestamp = 0;
		volatile bool Pending = false;
		volatile bool Double = false;

		void Clear()
		{
			Pending = false;
			Double = false;
		}
	};

	struct TxEventStruct
	{
		volatile uint32_t Timestamp = 0;
		volatile bool Pending = false;
		volatile bool Double = false;

		void Clear()
		{
			Pending = false;
			Double = false;
		}
	};

	struct RxEventStruct
	{
		volatile uint32_t Timestamp = 0;
		volatile uint8_t Size = 0;
		volatile int16_t Rssi = 0;
		volatile bool Double = false;

		const bool Pending()
		{
			return Size > 0;
		}

		void Clear()
		{
			Size = 0;
		}
	};

private:

	static constexpr uint8_t TX_TIMEOUT_MILLIS = 8;


	ILoLaTransceiverListener* Listener = nullptr;

private:
	void (*RadioInterrupt)(void) = nullptr;

private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	InterruptEventStruct RadioEvent{};
	TxEventStruct TxEvent{};
	RxEventStruct RxEvent{};

	uint32_t TxStartTimestamp = 0;
	uint32_t RxStartTimestamp = 0;
	uint8_t CurrentChannel = 0;

	uint8_t RxPending = 0;
	uint8_t RxPendingRssi = 0;
	volatile bool TxPending = false;

	bool HopPending = false;

public:
	Si446xTransceiver(Scheduler& scheduler)
		: ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
	{
		pinMode(InterruptPin, INPUT);
		pinMode(CsPin, INPUT);
		pinMode(SdnPin, INPUT);

#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, LOW);
		pinMode(RX_TEST_PIN, OUTPUT);
#endif

#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, LOW);
		pinMode(TX_TEST_PIN, OUTPUT);
#endif
	}

	void SetupInterrupt(void (*onRadioInterrupt)(void))
	{
		RadioInterrupt = onRadioInterrupt;
	}

public:
	void OnRadioInterrupt()
	{
		if (RadioEvent.Pending)
		{
			RadioEvent.Double = true;
		}
		else
		{
			RadioEvent.Timestamp = micros();
			RadioEvent.Pending = true;
		}
		Task::enable();
	}

	void OnTxInterrupt()
	{
#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, LOW);
#endif
		if (TxEvent.Pending)
		{
			TxEvent.Double = true;
		}
		else
		{
			TxEvent.Timestamp = micros();
			TxEvent.Pending = true;
		}
		Task::enable();
	}


	void OnPreRxInterrupt()
	{
#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, HIGH);
#endif

		if (TxPending)
		{
			RxEvent.Double = true;
			return;
		}

		if (RxEvent.Pending())
		{
			RxEvent.Double = true;
		}
		else
		{
			RxEvent.Timestamp = micros();
		}
		Task::enable();
	}

	void OnPostRxInterrupt(const uint8_t size, const int16_t rssi)
	{
#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, HIGH);
#endif

		if (RxEvent.Pending())
		{
			RxEvent.Double = true;
		}
		else
		{
			if (size > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
			{
				Listener->OnRxLost(RxEvent.Timestamp);
			}
			else
			{
				RxEvent.Size = size;
				RxEvent.Rssi = rssi;
			}
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
			&& RadioInterrupt != nullptr
			&& LoLaSi4463Config::ValidateConfig(MODEM_2_1))
		{
			// Set pins again, in case Transceiver has been reset.
			DisableInterrupt();
			pinMode(InterruptPin, INPUT_PULLUP);
			pinMode(CsPin, OUTPUT);
			pinMode(SdnPin, OUTPUT);

			Si446x_init();

			// Enable packet RX begin and packet sent callbacks
			Si446x_setupCallback(
				SI446X_CBS_SENT
				| SI446X_CBS_RXBEGIN
				| SI446X_CBS_RXCOMPLETE
				| SI446X_CBS_RXINVALID
				, 1);

			// Sleep until first command.
			Si446x_sleep();

			// Set random channel to start with.
			CurrentChannel = UINT8_MAX;

			// Start listening to IRQ.
			EnableInterrupt();

			Task::enable();

			return true;
		}

		return false;
	}

	virtual const bool Stop() final
	{
		Task::disable();

		return true;
	}

	virtual const bool TxAvailable() final
	{
		return !RadioEvent.Pending && !TxPending && !TxEvent.Pending && !RxEvent.Pending();
	}

	virtual const uint32_t GetTransceiverCode()
	{
		return (uint32_t)TRANSCEIVER_ID
			| (uint32_t)LoLaSi4463Config::GetConfigId(MODEM_2_1) << 16
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
#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, HIGH);
#endif
		if (TxAvailable())
		{
			CurrentChannel = GetRealChannel(channel);

			TxPending = true;

			TxStartTimestamp = millis();
			Task::enable();

			return 1 == Si446x_TX((uint8_t*)data, packetSize, CurrentChannel, si446x_state_t::SI446X_STATE_RX);
		}

		return false;
	}

	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		const uint8_t realChannel = GetRealChannel(channel);

		if (CurrentChannel != realChannel) {
			CurrentChannel = realChannel;
			HopPending = true;
			Task::enable();
		}
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		if (packetSize < LoLaPacketDefinition::MIN_PACKET_SIZE)
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
		if (packetSize < LoLaPacketDefinition::MIN_PACKET_SIZE)
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
		// Process pending Radio events.
		Si446x_SERVICE();

		if (RadioEvent.Pending)
		{
			if (RadioEvent.Double)
			{
#if defined(DEBUG_LOLA)
				Serial.println(F("Si446x Double Interrupt Event"));
#endif
			}
			else
			{
				//TODO: Ask Radio what event triggered and OnRxEvent/OnTxEvent
			}

			RadioEvent.Clear();
		}

		if (TxEvent.Pending)
		{
			if (TxEvent.Double)
			{
#if defined(DEBUG_LOLA)
				Serial.println(F("Si446x Double TxEvent"));
#endif
			}

			if (TxPending)
			{
				Listener->OnTx();
				TxPending = false;
			}
			else
			{
#if defined(DEBUG_LOLA)
				Serial.println(F("Si446x Tx Unexpected TxEvent."));
#endif
			}

			TxEvent.Clear();
			HopPending = true;
		}

		// Distribute any pending packets before responding to RxEvent.
		if (RxPending > 0)
		{
			Listener->OnRx(InBuffer, RxStartTimestamp, RxPending, RxPendingRssi);
			RxPending = 0;
		}

		if (RxEvent.Pending())
		{
			if (RxEvent.Double)
			{
#if defined(DEBUG_LOLA)
				Serial.print(F("Si446x Double RxEvent."));
#endif
				// TODO: Replace with clear Rx FIFO.
				Si446x_read(InBuffer, RxEvent.Size);
				Listener->OnRxLost(RxEvent.Timestamp);
			}
			else
			{
				Si446x_read(InBuffer, RxEvent.Size);

				if (RxEvent.Size < LoLaPacketDefinition::MIN_PACKET_SIZE)
				{
					// Invalid size.
					Listener->OnRxLost(RxEvent.Timestamp);
				}
				else
				{
					// Reading the data over SPI already took some time.
					// The buffered packet will be distributed on next run instead.
					// Since we have to double buffer the input, might as well use it to avoid skipping packets.
					RxPending = RxEvent.Size;
					RxPendingRssi = GetNormalizedRssi(RxEvent.Rssi);
					RxStartTimestamp = RxEvent.Timestamp;
				}
			}
			RxEvent.Clear();
			HopPending = true;
		}

		if (TxPending
			&& (millis() - TxStartTimestamp > TX_TIMEOUT_MILLIS))
		{
			TxPending = false;
#if defined(DEBUG_LOLA)
			Serial.print(F("Si446x TxPending timeout."));
#endif
		}

		if (HopPending)
		{
			HopPending = false;
#if defined(DEBUG_LOLA)
			if (CurrentChannel == UINT8_MAX)
			{
				Serial.print(F("Si446x Missed Rx channel on hop."));
				CurrentChannel = 0;
				Task::enable();
				return true;
			}
#endif
			Si446x_RX(CurrentChannel);
		}

		if (RxPending > 0
			|| RxEvent.Pending()
			|| TxPending || RadioEvent.Pending || TxEvent.Pending)
		{
			Task::enable();
		}
		else
		{
#if defined(SI446X_INTERRUPTS)
			Task::disable();
#else
			Task::enable();
#endif
		}

		return true;
	}


private:
	/// </summary>
	/// Converts the LoLa abstract channel into a real channel index for the radio.
	/// </summary>
	/// <param name="abstractChannel">LoLa abstract channel [0;255].</param>
	/// <returns>Returns the real channel to use [0;(ChannelCount-1)].</returns>
	const uint8_t GetRealChannel(const uint8_t abstractChannel)
	{
		return (abstractChannel % ChannelCount);
	}

	const uint8_t GetNormalizedRssi(const int16_t rssi)
	{
		return ((150 - rssi) * UINT8_MAX) / 90;
	}

private:
	void EnableInterrupt()
	{
		//attachInterrupt(digitalPinToInterrupt(InterruptPin), RadioInterrupt, FALLING);
	}

	void DisableInterrupt()
	{
		//detachInterrupt(digitalPinToInterrupt(InterruptPin));
	}
};
#endif