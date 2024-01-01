// nRF24Transceiverr.h
// For use with nRF24L01+.

#ifndef _LOLA_NRF24_TRANSCEIVER_h
#define _LOLA_NRF24_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <SPI.h>

#include "nRF24Support.h"

#include <ILoLaTransceiver.h>

// https://github.com/nRF24/RF24
#define RF24_SPI_PTR
#include <RF24.h>

/// <summary>
/// TODO: Optimize hop by checking if already on the selected channel, before issuing command to IC.
/// TODO: DataRate RF24_2MBPS is specified to need 2 MHz of channel separation.
/// TODO: Make SPI speed configurable.
/// TODO: nRF Reading pipe drops every 4th packet if CRC is not enabled.
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

	static constexpr uint32_t EVENT_TIMEOUT_MILLIS = 10;

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
	const uint16_t TX_DELAY_MIN = nRF24Support::NRF_TIMINGS[DataRate].TxDelayMin;
	const uint16_t TX_DELAY_RANGE = nRF24Support::NRF_TIMINGS[DataRate].TxDelayMax - TX_DELAY_MIN;
	const uint16_t RX_DELAY_MIN = nRF24Support::NRF_TIMINGS[DataRate].RxDelayMin;
	const uint16_t RX_DELAY_RANGE = nRF24Support::NRF_TIMINGS[DataRate].RxDelayMax - RX_DELAY_MIN;
	const uint16_t RX_HOP_DURATION = TX_DELAY_MIN;

	//The SPI interface is designed to operate at a maximum of 10 MHz.
#if defined(ARDUINO_ARCH_AVR)
	static const uint32_t NRF24_SPI_SPEED = 8000000;
#elif defined(ARDUINO_ARCH_STM32F1)
	static const uint32_t NRF24_SPI_SPEED = 16000000;
#elif defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
	static const uint32_t NRF24_SPI_SPEED = 16000000;
#else
	static const uint32_t NRF24_SPI_SPEED = RF24_SPI_SPEED;
#endif

private:
	nRF24Support::EventStruct Event{};
	nRF24Support::PacketEventStruct PacketEvent{};

	uint8_t CurrentChannel = 0;

private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	ILoLaTransceiverListener* Listener = nullptr;

	void (*OnInterrupt)(void) = nullptr;

	uint8_t RssiHistory = 0;

private:
	bool HopPending = false;

	bool TxPending = false;
	uint32_t TxStart = 0;

private:
	SPIClass* SpiInstance;
	RF24 Radio;

public:
	nRF24Transceiver(Scheduler& scheduler, SPIClass* spiInstance)
		: ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, SpiInstance(spiInstance)
		, Radio((rf24_gpio_pin_t)CePin, (rf24_gpio_pin_t)CsPin, NRF24_SPI_SPEED)
	{
		pinMode(InterruptPin, INPUT);
		pinMode(CePin, INPUT);
		pinMode(CsPin, INPUT);
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
#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, HIGH);
#endif
		if (Event.Pending)
		{
			// Double event detected.
			Event.DoubleEvent = true;
		}
		else
		{
			Event.Timestamp = micros();
			Event.Pending = true;
		}
		Task::enable();
	}

public:	// ILoLaTransceiver overrides.

	/// <summary>
	/// Set up the transceiver listener that will handle all packet events.
	/// </summary>
	/// <param name="listener"></param>
	/// <returns>True on success.</returns>
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr;
	}

	/// <summary>
	/// Boot up the device and start in working mode.
	/// </summary>
	/// <returns>True on success.</returns>
	virtual const bool Start() final
	{
		if (Listener != nullptr && OnInterrupt != nullptr)
		{
			// Set pins again, in case Transceiver has been reset.
			DisableInterrupt();
			pinMode(InterruptPin, INPUT_PULLUP);
			pinMode(CsPin, OUTPUT);
			pinMode(CePin, OUTPUT);

			// Radio driver handles CePin and CsPin setup.
			if (!Radio.begin(SpiInstance))
			{
				Serial.println("Radio.begin failed.");
				return false;
			}

			// Ensure the IC is present.
			if (!Radio.isChipConnected())
			{
				Serial.println("No chip found_2.");
				return false;
			}

			// LoLa already ensures integrity AND authenticity with its own MAC-CRC.
			//Radio.setCRCLength(RF24_CRC_DISABLED);
			//Radio.disableCRC();
			// nRF Reading pipe drops every 4th packet if CRC is not enabled.
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
			Radio.setAddressWidth(AddressSize);

			// Interrupt approach requires no delays.
			Radio.csDelay = 0;
			Radio.txDelay = 0;

			// Enable Tx and Rx interrupts. TxFail are disabled when Ack are disabled.
			Radio.maskIRQ(false, true, false);

			// Open radio pipes.
			Radio.openReadingPipe(AddressPipe, nRF24Support::HighEntropyShortAddress);
			Radio.openWritingPipe(nRF24Support::HighEntropyShortAddress);

			// Initialize with low power level, let power manager adjust it after boot.
			Radio.setPALevel(RF24_PA_LOW);

			// Clear RSSI history.
			RssiHistory = 0;

			// Set random channel to start with.
			CurrentChannel = 0;
			HopPending = true;
			Task::enable();

			// Start listening to IRQ.
			EnableInterrupt();

			if (Radio.failureDetected)
			{
#if defined(DEBUG_LOLA)
				Serial.println(F("nRF24 Failure detected."));
#endif
				return false;
			}

			return true;
		}
		else
		{
			return false;
		}
	}

	/// <summary>
	/// Stop the device and turn off power if possible.
	/// </summary>
	/// <returns>True if transceiver was running.</returns>
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
		// Process pending interrupt events to trigger aware PacketEvent.
		if (Event.Pending && !PacketEvent.Pending())
		{
			Radio.whatHappened(PacketEvent.TxOk, PacketEvent.TxFail, PacketEvent.RxReady);
			if (Event.DoubleEvent)
			{
#if defined(DEBUG_LOLA)
				Serial.print(millis());
				Serial.print(F(": "));
				Serial.println(F("Got Double Event!"));
#endif
			}
			else
			{
				PacketEvent.Timestamp = Event.Timestamp;
			}
			Event.Clear();
		}

		// Process Rx event.
		if (PacketEvent.RxReady)
		{
			if (Radio.available())
			{
				const int8_t packetSize = Radio.getDynamicPayloadSize();

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
					Listener->OnRx(InBuffer, PacketEvent.Timestamp - GetRxDelay(packetSize), packetSize, GetRxRssi());
				}
				else
				{
					// Corrupt packet, flush.
					Radio.flush_rx();
#if defined(DEBUG_LOLA)
					Serial.print(millis());
					Serial.print(F(": "));
					Serial.println(F("Corrupt Rx."));
#endif
					Listener->OnRxLost(PacketEvent.Timestamp - GetRxDelay(packetSize));
				}
				PacketEvent.RxReady = false;
				Task::enable();

				return true;
			}
			else
			{
				// Invalid packet, flush.
				Radio.flush_rx();
				PacketEvent.RxReady = false;
#if defined(DEBUG_LOLA)
				Serial.print(millis());
				Serial.print(F(": "));
				Serial.println(F("Got Rx Event but no data."));
#endif
			}
		}
		// Process Tx event.
		else if (PacketEvent.TxOk)
		{
			HopPending = true;

			if (TxPending)
			{
				if (!PacketEvent.TxOk)
				{
#if defined(DEBUG_LOLA)
					Serial.print(millis());
					Serial.print(F(": "));
					Serial.println(F("Tx Collision."));
#endif
				}
				Listener->OnTx();
			}
			else
			{
				// Unexpected interrupt.
				Radio.flush_rx();
				Radio.flush_tx();
#if defined(DEBUG_LOLA)
				Serial.print(millis());
				Serial.print(F(": "));
				Serial.println(F("Unexpected Tx interrupt."));
#endif
			}
			PacketEvent.TxOk = false;
			PacketEvent.TxFail = false;
			TxPending = false;
		}

		if (TxPending && ((millis() - TxStart) > EVENT_TIMEOUT_MILLIS))
		{
#if defined(DEBUG_LOLA)
			Serial.print(millis());
			Serial.print(F(": "));
			Serial.println(F("Tx Timeout."));
#endif
			TxPending = false;
			HopPending = true;
			Listener->OnTx();
		}

		if (HopPending)
		{
			if (!TxPending && !Event.Pending)
			{
				Radio.setChannel(CurrentChannel);
				Radio.flush_rx();
				Radio.startListening();

				HopPending = false;
			}
		}

#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, LOW);
#endif

		if (TxPending || Event.Pending || HopPending || PacketEvent.Pending())
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
	/// Inform the caller if the transceiver can currently perform a Tx.
	/// </summary>
	/// <returns></returns>
	virtual const bool TxAvailable() final
	{
		if (TxPending || Event.Pending || HopPending || PacketEvent.Pending())
		{
			return false;
		}

		// Check radio status for incoming Rx.Tx outgoing not required as TxPending already tracks it.
		return TxAvailableNow();
	}

	/// <summary>
	/// Tx/Transmit a packet at the indicated channel.
	/// Transceiver should auto-return to RX mode on same channel,
	///  but Rx() will be always called after OnTx() is called from transceiver.
	/// </summary>
	/// <param name="data">Raw packet data.</param>
	/// <param name="packetSize">Packet size.</param>
	/// <param name="channel">Normalized channel [0:255].</param>
	/// <returns>True if success.</returns>
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		if (TxAvailableNow())
		{
			Radio.stopListening();
			CurrentChannel = GetRawChannel(channel);
			Radio.setChannel(CurrentChannel);

			// Sending clears interrupt flags.
			// Wait for on Sent interrupt.
			TxPending = true;

			// Transmit packet.
			if (Radio.startWrite(data, packetSize, true))
			{
				TxStart = millis();
				Task::enable();
			}
			else
			{
				TxPending = false;
			}

			return TxPending;
		}

		return false;
	}

	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		const uint8_t rawChannel = GetRawChannel(channel);

		if (CurrentChannel != rawChannel) {
			CurrentChannel = rawChannel;
			HopPending = true;
			Task::enable();
		}
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
	/// <summary>
	/// No pending Rx data, best guarantee that the nRF hasn't started a Rx.
	/// </summary>
	/// <returns>True if can Tx now.</returns>
	const bool TxAvailableNow()
	{
		return Radio.isFifo(false, true);
	}

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
	static constexpr uint8_t GetRawChannel(const uint8_t abstractChannel)
	{
		return GetRealChannel<ChannelCount>(abstractChannel);
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
	/// Turns packet history of 1-bit RSSI into abstract RSSI.
	/// Uses history of the latest 3 packets' RSSI to get more precision.
	/// </summary>
	/// <returns>Abstract RSSI [0;255].</returns>
	const uint8_t GetRxRssi()
	{
		// Check latest position first.
		uint8_t rssi = 0;
		if (RssiHistory & (1))
		{
			rssi += 1 + (((uint16_t)UINT8_MAX * 10) / 16);
		}

		if (RssiHistory & (1 << 1))
		{
			rssi += 1 + (((uint16_t)UINT8_MAX * 5) / 16);
		}

		if (RssiHistory & (1 << 2))
		{
			rssi += ((uint16_t)UINT8_MAX * 1) / 16;
		}

		return rssi;
	}
};
#endif