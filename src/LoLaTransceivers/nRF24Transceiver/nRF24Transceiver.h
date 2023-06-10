// nRF24Transceiverr.h
// For use with nRF24L01.

#ifndef _LOLA_NRF24_TRANSCEIVER_h
#define _LOLA_NRF24_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <SPI.h>

// https://github.com/nRF24/RF24
#define RF24_SPI_PTR
#include <RF24.h>

#include <ILoLaTransceiver.h>


/// <summary>
/// Used to estimate Tx duration and Rx compensation.
/// </summary>
struct nRF24Timings
{
	uint16_t RxDelayMin;
	uint16_t RxDelayMax;
	uint16_t TxDelayMin;
	uint16_t TxDelayMax;
};

/// <summary>
/// Timings ordered by Baudarate enum value.
/// [RF24_1MBPS|RF24_2MBPS|RF24_250KBPS] 
/// </summary>
static const nRF24Timings TimingsNRF[3] = { {220, 470, 60, 116}, {190, 310, 60, 116}, {400, 1390, 60, 120} };


/// <summary>
/// TODO: Fix 1st RX always fails (no interrupt).
/// TODO: Optimize hop by checking if already on the selected channel, before issuing command to IC.
/// TODO: MAke SPI speed configurable.
/// </summary>
/// <typeparam name="CePin"></typeparam>
/// <typeparam name="CsPin"></typeparam>
/// <typeparam name="InterruptPin"></typeparam>
/// <typeparam name=DataRate"">RF24_250KBPS, RF24_1MBPS or RF24_2MBPS</typeparam>
template<const uint8_t CePin,
	const uint8_t CsPin,
	const uint8_t InterruptPin,
	const rf24_datarate_e DataRate = RF24_250KBPS>
class nRF24Transceiver : private Task, public virtual ILoLaTransceiver
{
private:
	// 0 to 125 are valid channels.
	static constexpr uint8_t ChannelCount = 126;

	// Only one pipe for protocol.
	static constexpr uint8_t AddressPipe = 0;

	// Generic addressing, identifies protocol.
	static constexpr uint8_t AddressSize = 4;
	const uint8_t BaseAddress[AddressSize] = { 'L', 'o', 'L' , 'a' };

	// The used timings' constants, based on baudrate.
	const uint16_t TX_DELAY_MIN = TimingsNRF[DataRate].TxDelayMin;
	const uint16_t TX_DELAY_RANGE = TimingsNRF[DataRate].TxDelayMax - TX_DELAY_MIN;
	const uint16_t RX_DELAY_MIN = TimingsNRF[DataRate].RxDelayMin;
	const uint16_t RX_DELAY_RANGE = TimingsNRF[DataRate].RxDelayMax - RX_DELAY_MIN;

	static constexpr uint32_t EVENT_TIMEOUT_MICROS = 5000;
	
	struct EventStruct
	{
		uint32_t Timestamp = 0;
		bool TxOk = false;
		bool TxFail = false;
		bool RxReady = false;

		const bool Pending()
		{
			return TxOk || TxFail || RxReady;
		}

		void Clear()
		{
			TxOk = false;
			TxFail = false;
			RxReady = false;
		}
	};

private:
	RF24 Radio;

	EventStruct Event;

private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	ILoLaTransceiverListener* Listener = nullptr;

	void (*OnInterrupt)(void) = nullptr;

private:
	volatile bool InterruptTimestamp = 0;
	volatile bool DoubleEvent = false;
	volatile bool InterruptPending = false;

	bool TxPending = false;

public:
	nRF24Transceiver(Scheduler& scheduler)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, ILoLaTransceiver()
		, Radio(CePin, CsPin)
		, Event()
	{
		pinMode(InterruptPin, INPUT);
		pinMode(CePin, INPUT);
		pinMode(CsPin, INPUT);

#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, LOW);
		pinMode(RX_TEST_PIN, OUTPUT);
#endif

#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, LOW);
		pinMode(TX_TEST_PIN, OUTPUT);
#endif
	}

	void SetupInterrupt(void (*onInterrupt)(void))
	{
		OnInterrupt = onInterrupt;
	}

	/// <summary>
	/// To be called on nRF interrupt.
	/// </summary>
	void OnRfInterrupt()
	{
		if (InterruptPending)
		{
			// Double event detected.
			DoubleEvent = true;
		}
		else
		{
			InterruptTimestamp = micros();
			InterruptPending = true;
			Task::enable();
		}
	}

public:
	virtual bool Callback() final
	{
		if (InterruptPending)
		{
			if (DoubleEvent)
			{
				DoubleEvent = false;
				InterruptPending = false;
				Event.Clear();
				if (TxPending)
				{
					TxPending = false;
					Listener->OnTx();
				}
#if defined(DEBUG_LOLA)
				Serial.print(millis());
				Serial.print(F(": "));
				Serial.println(F("Got Double Event!"));
#endif
				return true;
			}

			if (!Event.Pending())
			{
				Radio.whatHappened(Event.TxOk, Event.TxFail, Event.RxReady);

				if (TxPending)
				{
					if (!Event.TxOk && !Event.TxFail)
					{
#if defined(DEBUG_LOLA)
						Serial.print(millis());
						Serial.print(F(": "));
						Serial.println(F("Force Tx Ok."));
#endif
						Event.TxOk = true;
						Event.TxFail = false;
					}
				}

				if (Event.Pending())
				{
					Event.Timestamp = InterruptTimestamp;
					InterruptPending = false;
				}
				else if ((micros() - InterruptTimestamp) > EVENT_TIMEOUT_MICROS)
				{
#if defined(DEBUG_LOLA)
					Serial.print(millis());
					Serial.print(F(": "));
					Serial.println(F("Event Timeout."));
#endif
					InterruptPending = false;
				}
			}
		}

		if (Event.TxOk || Event.TxFail)
		{
			if (Event.TxFail)
			{
				//TODO: Log.
				Radio.flush_tx();
			}

			Event.TxOk = false;
			Event.TxFail = false;

			if (TxPending)
			{
				TxPending = false;
				Listener->OnTx();
			}
			else
			{
				// Unexpected interrupt.
#if defined(DEBUG_LOLA)
				Serial.print(millis());
				Serial.print(F(": "));
				Serial.println(F("Unexpected Tx interrupt."));
#endif
			}
		}

		if (Event.RxReady)
		{
			Event.RxReady = false;
#ifdef RX_TEST_PIN
			digitalWrite(RX_TEST_PIN, HIGH);
			digitalWrite(RX_TEST_PIN, LOW);
#endif

			if (Radio.available())
			{
				const int8_t packetSize = Radio.getDynamicPayloadSize();

				if (packetSize >= LoLaPacketDefinition::MIN_PACKET_SIZE)
				{
					// Copy packet from radio to InBuffer.
					Radio.read(InBuffer, packetSize);

					// Radio has no RSSI measure, all reception is perfect reception.
					// Rx Interrupt only occurs when packet has been fully received, the timestamp must be compensated with RxDelay.
					Listener->OnRx(InBuffer, Event.Timestamp - GetRxDelay(packetSize), packetSize, UINT8_MAX);
				}
				else
				{
					// Invalid packet.
					//if (packetSize < 1)
					// Corrupt packet has been flushed.
					Radio.flush_rx();

					Listener->OnRxLost(Event.Timestamp);
				}

				Task::enable();
				return true;
			}
		}

		if (InterruptPending || TxPending || Event.Pending())
		{
			Task::enable();
			return true;
		}
		else
		{
			Task::delay(1);
			return false;
		}
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
		if (Listener != nullptr)
		{
			// Set pins again, in case Transceiver has been reset.
			DisableInterrupt();
			pinMode(InterruptPin, INPUT);
			pinMode(CsPin, OUTPUT);
			pinMode(CePin, OUTPUT);

			// Radio driver handles CePin and CsPin setup.
			if (!Radio.begin(CePin, CsPin))
			{
				return false;
			}

			// Ensure the IC is present.
			if (!Radio.isChipConnected())
			{
				return false;
			}

			// LoLa already ensures integrity AND authenticity with its own MAC-CRC.
			Radio.setCRCLength(RF24_CRC_DISABLED);
			Radio.disableCRC();

			// LoLa doesn't specify any data rate, this should be defined in the user Link Protocol.
			if (!Radio.setDataRate(DataRate))
			{
				return false;
			}

			// LoLa requires no auto ack.
			Radio.setAutoAck(false);
			Radio.setRetries(0, 0);
			Radio.disableAckPayload();

			// Minimize total packet size to reduce on-air time.
			Radio.enableDynamicPayloads();

			// Set address witdh to minimum, as it is not required to distinguish between devices.
			Radio.setAddressWidth(4);

			// Interrupt approach requires no delays.
			Radio.csDelay = 0;
			Radio.txDelay = 0;

			// Enable Tx and Rx interrupts. TxFail are disabled when Ack are disabled.
			Radio.maskIRQ(false, true, false);

			// Open radio pipes.
			Radio.openReadingPipe(AddressPipe, BaseAddress);
			Radio.openWritingPipe(BaseAddress);

			// Initialize with low power level, let power manager adjust it after boot.
			Radio.setPALevel(RF24_PA_MIN);

			// Set random channel to start with.
			Rx(0);
			Task::enable();

			// Start listening to IRQ.
			EnableInterrupt();

			return true;
		}
		else
		{
			return false;
		}
	}

	virtual const bool Stop() final
	{
		Radio.stopListening();
		Radio.closeReadingPipe(AddressPipe);
		DisableInterrupt();
		Task::disable();

		return true;
	}

	/// <summary>
	/// </summary>
	/// <returns></returns>
	virtual const bool TxAvailable() final
	{
		//TODO: Check radio status.
		return !TxPending && !InterruptPending;
	}

	/// <summary>
	/// Packet copy to serial buffer, and start transmission.
	/// The first byte is the packet size, excluding the size byte.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	/// 
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		if (TxAvailable())
		{
#ifdef TX_TEST_PIN
			digitalWrite(TX_TEST_PIN, HIGH);
			digitalWrite(TX_TEST_PIN, LOW);
#endif
			Radio.stopListening();

			// Sending clears interrupt flags.
			// Wait for on Sent interrupt.
			TxPending = true;
			Event.TxOk = false;
			Event.TxFail = false;
			Task::enable();

			// Transmit packet.
			Radio.startFastWrite(data, packetSize, 0);

			return true;
		}

		return false;
	}

	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		Radio.setChannel(GetRealChannel(channel));
		Radio.startListening();
		Task::enable();
	}

	/// <summary>
	/// </summary>
	/// <param name="packetSize">Number of bytes in the packet.</param>
	/// <returns>The expected transmission duration in microseconds.</returns>
	virtual const uint32_t GetTransmitDurationMicros(const uint8_t packetSize) final
	{
		//Measure Tx duration value.
		return GetTxDelay(packetSize);
	}

private:
	void EnableInterrupt()
	{
		attachInterrupt(digitalPinToInterrupt(InterruptPin), OnInterrupt, FALLING);
	}

	void DisableInterrupt()
	{
		detachInterrupt(digitalPinToInterrupt(InterruptPin));
	}

	/// </summary>
	/// Converts the LoLa abstract channel into a real channel index for the radio.
	/// </summary>
	/// <param name="abstractChannel">LoLa abstract channel [0;255].</param>
	/// <returns>Returns the real channel to use [0;(ChannelCount-1)].</returns>
	const uint8_t GetRealChannel(const uint8_t abstractChannel)
	{
		return (abstractChannel % ChannelCount);
	}

	/// <summary>
	/// </summary>
	/// <param name="packetSize"></param>
	/// <returns></returns>
	const uint_least16_t GetRxDelay(const uint8_t packetSize)
	{
		return RX_DELAY_MIN + ((((uint32_t)RX_DELAY_RANGE) * packetSize) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	}

	/// <summary>
	/// How long the from Tx command start to on-air.
	/// </summary>
	/// <param name="packetSize"></param>
	/// <returns></returns>
	const uint_least16_t GetTxDelay(const uint8_t packetSize)
	{
		return TX_DELAY_MIN + ((((uint32_t)TX_DELAY_RANGE) * packetSize) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	}
};
#endif