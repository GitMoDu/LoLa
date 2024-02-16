// Sx1Transceiver.h
#ifndef _LOLA_SX1_TRANSCEIVER_h
#define _LOLA_SX1_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <SPI.h>

#include "Sx12Support.h"
#include <SX126x.h>

#include <ILoLaTransceiver.h>


/// <summary>
/// For use with Sx LoRa Radios: SX127x, SX126x
/// </summary>
/// <typeparam name="CsPin"></typeparam>
/// <typeparam name="ResetPin"></typeparam>
/// <typeparam name="InterruptPin"></typeparam>
template<const uint8_t CsPin,
	const uint8_t BusyPin,
	const uint8_t ResetPin,
	const uint8_t InterruptPin>
class Sx12Transceiver final
	: private Task, public virtual ILoLaTransceiver
{
private:
	static constexpr uint16_t TRANSCEIVER_ID = 0x5810;

	static constexpr uint32_t EVENT_TIMEOUT_MILLIS = 10;

	// ? RF channels, ? KHz steps.
	static constexpr uint8_t ChannelCount = 1;

	//The SPI interface is designed to operate at a maximum of 10 MHz.
#if defined(ARDUINO_ARCH_AVR)
	static constexpr uint32_t SX1_SPI_SPEED = 8000000;
#elif defined(ARDUINO_ARCH_STM32F1)
	static constexpr uint32_t SX1_SPI_SPEED = 16000000;
#elif defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
	static constexpr uint32_t SX1_SPI_SPEED = 16000000;
#else
	static constexpr uint32_t SX1_SPI_SPEED = 8000000;
#endif

private:
	Sx12::EventStruct Event{};
	Sx12::PacketEventStruct PacketEvent{};

private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	ILoLaTransceiverListener* Listener = nullptr;

	void (*OnInterrupt)(void) = nullptr;

private:
	bool HopPending = false;

	volatile bool TxPending = false;
	volatile bool RxPending = false;
	volatile uint32_t RxStart = 0;
	uint32_t TxStart = 0;

private:
	SPIClass* SpiInstance;
	SX126x Radio;

public:
	Sx12Transceiver(Scheduler& scheduler, SPIClass* spiInstance)
		: ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, SpiInstance(spiInstance)
		, Radio()
	{
		pinMode(InterruptPin, INPUT);
		pinMode(CsPin, INPUT);
		pinMode(BusyPin, INPUT);
		pinMode(ResetPin, INPUT);
	}

	void OnRadioInterrupt()
	{
		Task::enable();
	}

	void SetupInterrupt(void (*onRadioInterrupt)(void))
	{
		OnInterrupt = onRadioInterrupt;
	}

	//	/// <summary>
	//	/// To be called on Radio interrupt.
	//	/// </summary>
	//	void OnRfInterrupt()
	//	{
	//#ifdef RX_TEST_PIN
	//		digitalWrite(RX_TEST_PIN, HIGH);
	//#endif
	//		if (Event.Pending)
	//		{
	//			// Double event detected.
	//			Event.DoubleEvent = true;
	//		}
	//		else
	//		{
	//			Event.Timestamp = micros();
	//			Event.Pending = true;
	//		}
	//		Task::enable();
	//	}

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
		if (Listener != nullptr
			&& OnInterrupt != nullptr)
		{
			// Set pins again, in case Transceiver has been reset.
			DisableInterrupt();
			pinMode(InterruptPin, INPUT_PULLUP);
			pinMode(CsPin, OUTPUT);
			pinMode(ResetPin, OUTPUT);
			pinMode(BusyPin, OUTPUT);

			// Radio driver handles Pin setup?
			//SpiInstance->begin(9, 11, 10, CsPin);
			Radio.setPins(CsPin, ResetPin, BusyPin, InterruptPin, -1, -1);
			Radio.setSPI(*SpiInstance, SX126X_SPI_FREQUENCY);

			// Ensure the IC is present.
			if (!Radio.begin())
			{
				Serial.println(F("!Radio.begin()"));
				return false;
			}

			Radio.setFrequency(868000000);

			Radio.setFskModulation(
				Sx12::GetBrFromBitRate(117300), // LoLa doesn't specify any data rate, this should be defined in the user Link Protocol.
				(uint8_t)Sx12::PulseShape::GaussianBt05,
				(uint8_t)Sx12::RxBandWitdh::Bw117300,
				Sx12::OSCILATOR_DEVIATION);

			Radio.setFskPacket(
				Sx12::PRE_AMBLE_LENGTH, // Minimize total packet size to reduce on-air time.				
				Sx12::PRE_AMBLE_DETECT,
				Sx12::SYNC_LENGTH,
				(uint8_t)Sx12::AddrCompare::NoFiltering, // Address is not required to distinguish between devices.
				(uint8_t)Sx12::PacketType::VariableLength,
				LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE,
				(uint8_t)Sx12::CrcType::NoCrc, // LoLa already ensures integrity AND authenticity with its own MAC-CRC.
				(uint8_t)Sx12::Whitening::WhiteningEnabled
			);

			Radio.setFskSyncWord((uint8_t*)&Sx12::SYNC_WORD, 1);

			// Initialize the Radio callbacks
			Radio.onReceive(*OnInterrupt);
			Radio.onTransmit(*OnInterrupt);

			// Initialize with low power level, let power manager adjust it after boot.
			Radio.setTxPower(0, SX126X_TX_POWER_SX1262);
			Radio.setRxGain(LORA_RX_GAIN_POWER_SAVING);
			Radio.setRxGain(LORA_RX_GAIN_BOOSTED);

			// DIO1 as Interrupt pin.
			Radio.setRfIrqPin(1);

			// Set random channel to start with.
			HopPending = true;
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

	/// <summary>
	/// Stop the device and turn off power if possible.
	/// </summary>
	/// <returns>True if transceiver was running.</returns>
	virtual const bool Stop() final
	{
		DisableInterrupt();
		Task::disable();

		return true;
	}

public:
	virtual bool Callback() final
	{
		if (RxPending)
		{
			RxPending = false;
			Serial.print(F("Got Rx at "));
			Serial.print(RxStart);
		}

		if (TxPending)
		{
			TxPending = false;
			Serial.print(F("Tx done."));
		}

		if (RxPending || TxPending)
		{
			Task::forceNextIteration();
			return true;
		}
		else
		{
			Task::disable();
			return false;
		}
	}


	/// <summary>
	/// Inform the caller if the transceiver can currently perform a Tx.
	/// </summary>
	/// <returns></returns>
	virtual const bool TxAvailable() final
	{
		//if (TxPending || Event.Pending || HopPending || PacketEvent.Pending())
		//{
		//	return false;
		//}

		// Check radio status.
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
		const uint8_t currentMode = Radio.getMode();

		if (currentMode == SX126X_STATUS_MODE_RX
			|| currentMode == SX126X_STATUS_MODE_STDBY_RC)
		{
			Radio.beginPacket();
			Radio.write((uint8_t*)data, (uint8_t)packetSize);

			return Radio.endPacket();
		}
		else
		{
			Serial.print(F("STATE: "));
			Serial.println(Radio.getMode());
		}

		return false;

	}

	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		while (Radio.available())
		{
			Radio.read();
		}
		Radio.listen(0xFFFFFF, 0);
	}

	virtual const uint32_t GetTransceiverCode() final
	{
		return (uint32_t)TRANSCEIVER_ID
			| (uint32_t)ChannelCount << 24;
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return 0;
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		return 0;
	}

private:
	const bool TxAvailableNow()
	{
		return !Radio.busyCheck(0);
	}

	void EnableInterrupt()
	{
		//attachInterrupt(digitalPinToInterrupt(InterruptPin), OnInterrupt, FALLING);
	}

	void DisableInterrupt()
	{
		//detachInterrupt(digitalPinToInterrupt(InterruptPin));
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
};
#endif