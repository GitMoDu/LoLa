// nRF24Transceiverr.h
// For use with nRF24L01+.

#ifndef _LOLA_NRF24_TRANSCEIVER_h
#define _LOLA_NRF24_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <SPI.h>

// https://github.com/nRF24/RF24
#define RF24_SPI_PTR
#include <RF24.h>

#include "nRF24Support.h"

#include <ILoLaTransceiver.h>

/// <summary>
/// TODO: Optimize hop by checking if already on the selected channel, before issuing command to IC.
/// TODO: DataRate RF24_2MBPS is specified to need 2 MHz of channel separation.
/// TODO: Make SPI speed configurable.
/// TODO: Rx is not working if no CRC is used.
/// </summary>
/// <typeparam name="CePin"></typeparam>
/// <typeparam name="CsPin"></typeparam>
/// <typeparam name="InterruptPin"></typeparam>
/// <typeparam name=DataRate"">RF24_250KBPS, RF24_1MBPS or RF24_2MBPS</typeparam>
template<const uint8_t CePin,
	const uint8_t CsPin,
	const uint8_t InterruptPin,
	const rf24_datarate_e DataRate = RF24_250KBPS>
class nRF24Transceiver final
	: private Task, public virtual ILoLaTransceiver
{
private:
	static constexpr uint16_t TRANSCEIVER_ID = 0x3F24;

	static constexpr uint32_t EVENT_TIMEOUT_MICROS = 3000;

	// 126 RF channels, 1 MHz steps.
	static constexpr uint8_t ChannelCount = 126;

	// 130us according to datasheet.
	static constexpr uint16_t RxHopDelay = 130;

	// Only one pipe for protocol.
	static constexpr uint8_t AddressPipe = 0;

	// Fixed addressing identifies protocol.
	static constexpr uint8_t AddressSize = 3;

	// The used timings' constants, based on baudrate.
	// Used to estimate Tx duration and Rx compensation.
	const uint16_t TX_DELAY_MIN = NRF_TIMINGS[DataRate].TxDelayMin;
	const uint16_t TX_DELAY_RANGE = NRF_TIMINGS[DataRate].TxDelayMax - TX_DELAY_MIN;
	const uint16_t RX_DELAY_MIN = NRF_TIMINGS[DataRate].RxDelayMin;
	const uint16_t RX_DELAY_RANGE = NRF_TIMINGS[DataRate].RxDelayMax - RX_DELAY_MIN;
	const uint16_t RX_HOP_DURATION = TX_DELAY_MIN;

private:
	RF24 Radio;

	EventStruct Event;

private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	ILoLaTransceiverListener* Listener = nullptr;

	void (*OnInterrupt)(void) = nullptr;

	uint8_t RssiHistory = 0;

private:
	volatile uint32_t InterruptTimestamp = 0;
	volatile bool DoubleEvent = false;
	volatile bool InterruptPending = false;

	bool TxPending = false;

public:
	nRF24Transceiver(Scheduler& scheduler)
		: ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
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
			//Radio.setCRCLength(RF24_CRC_DISABLED);
			//Radio.disableCRC();
			// Rx is not working if no CRC is used.
			Radio.setCRCLength(RF24_CRC_8);


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
			Radio.openReadingPipe(AddressPipe, HighEntropyShortAddress);
			Radio.openWritingPipe(HighEntropyShortAddress);

			// Initialize with low power level, let power manager adjust it after boot.
			Radio.setPALevel(RF24_PA_MIN);

			// Clear RSSI history.
			RssiHistory = 0;

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

public:
	virtual bool Callback() final
	{
		if (InterruptPending)
		{
			if (DoubleEvent)
			{
				DoubleEvent = false;
				InterruptPending = false;
				Radio.whatHappened(Event.TxOk, Event.TxFail, Event.RxReady);
				Event.Clear();
				if (TxPending)
				{
					TxPending = false;
					Radio.flush_tx();
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
						Event.TxOk = true;
						Event.TxFail = false;
#if defined(DEBUG_LOLA)
						Serial.print(millis());
						Serial.print(F(": "));
						Serial.println(F("Tx Collision, Rx lost."));
#endif
					}
				}
				else if (!Event.RxReady && (Event.TxOk || Event.TxFail))
				{
					// Unexpected interrupt.
#if defined(DEBUG_LOLA)
					Serial.print(millis());
					Serial.print(F(": "));
					Serial.println(F("Unexpected Tx interrupt."));
#endif
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
					Radio.whatHappened(Event.TxOk, Event.TxFail, Event.RxReady);
					Radio.flush_rx();
					Radio.flush_tx();
					Radio.startListening();
					Event.Clear();
					InterruptPending = false;
					if (TxPending)
					{
						TxPending = false;
						Radio.flush_tx();
						Listener->OnTx();
					}
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

				Event.Timestamp -= GetRxDelay(packetSize);
				if (packetSize >= LoLaPacketDefinition::MIN_PACKET_SIZE)
				{
					// Store 8 packet history of 1-bit RSSI. 
					RssiHistory <<= 1;
					if (Radio.testRPD())
					{
						RssiHistory |= 1; // Received power > -64 dBm.
					}

					// Read packet from radio to InBuffer.
					Radio.read(InBuffer, packetSize);

					// Rx Interrupt only occurs when packet has been fully received, so the timestamp must be compensated with RxDelay.
					Listener->OnRx(InBuffer, Event.Timestamp, packetSize, GetRxRssi());
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


	/// <summary>
	/// </summary>
	/// <returns></returns>
	virtual const bool TxAvailable() final
	{
		//TODO: Check radio status.
		return !TxPending && !InterruptPending && !Event.Pending();
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
			Radio.setChannel(GetRealChannel(channel));

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

	virtual const uint32_t GetTransceiverCode() final
	{
		uint8_t dataRateCode = 0;
		switch (DataRate)
		{
		case RF24_250KBPS:
			dataRateCode = 25;
			break;
		case RF24_1MBPS:
			dataRateCode = 100;
			break;
		case RF24_2MBPS:
			dataRateCode = 200;
		default:
			break;
		}
		return ((uint32_t)TRANSCEIVER_ID ^ AddressPipe ^ ((uint32_t)AddressSize << 8))
			| (uint32_t)dataRateCode << 16
			| (uint32_t)ChannelCount << 24;
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return GetTxDelay(packetSize);
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		return GetRxDelay(packetSize);
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
	constexpr uint8_t GetRealChannel(const uint8_t abstractChannel)
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

	/// <summary>
	/// Turns packet history of 1-bit RSSI into absctract RSSI.
	/// Uses history of the latest 3 packets' RSSI to get more precision.
	/// </summary>
	/// <returns>Abstract RSSI [0;255].</returns>
	const uint8_t GetRxRssi()
	{
		// Check latest position first.
		if (RssiHistory & (1))
		{
			if (RssiHistory & (1 << 2))
			{
				if (RssiHistory & (1 << 3))
				{
					return ((uint16_t)UINT8_MAX * 14) / 16;
				}
				else
				{
					return ((uint16_t)UINT8_MAX * 10) / 16;
				}
			}
			else
			{
				if (RssiHistory & (1 << 3))
				{
					return ((uint16_t)UINT8_MAX * 6) / 16;
				}
				else
				{
					return ((uint16_t)UINT8_MAX * 2) / 16;
				}
			}
		}
		else
		{
			if (RssiHistory & (1 << 2))
			{
				if (RssiHistory & (1 << 3))
				{
					return ((uint16_t)UINT8_MAX * 7) / 16;
				}
				else
				{
					return ((uint16_t)UINT8_MAX * 5) / 16;
				}
			}
			else
			{
				if (RssiHistory & (1 << 3))
				{
					return ((uint16_t)UINT8_MAX * 3) / 16;
				}
				else
				{
					return ((uint16_t)UINT8_MAX * 1) / 16;
				}
			}
		}
	}
};
#endif