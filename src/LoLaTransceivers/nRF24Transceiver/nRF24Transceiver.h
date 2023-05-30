// nRF24Transceiverr.h
// For use with nRF24L01.

#ifndef _LOLA_NRF24_TRANSCEIVER_h
#define _LOLA_NRF24_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>

// https://github.com/nRF24/RF24
#define RF24_SPI_PTR
#include <SPI.h>
#include <RF24.h>

/// <summary>
/// TODO: Optimize hop by checking if already on the selected channel, before issuing command to IC.
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
	// Plenty of channels for Hopping.
	static constexpr uint8_t ChannelCount = 126;

	//TODO: Adjust based on F_CPU or SPI speed?
	static constexpr uint8_t RadioCommsDelay = 50;

	static constexpr uint8_t AddressPipe = 0;

	static constexpr uint8_t AddressSize = 4;
	const uint8_t BaseAddress[AddressSize] = { 'L', 'o', 'L' , 'a'};

private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	ILoLaTransceiverListener* Listener = nullptr;

	void (*OnInterrupt)(void) = nullptr;

	//uint8_t CurrentChannel = 0;

	volatile bool InterruptTimestamp = 0;
	volatile bool InterruptPending = 0;
	bool DriverEnabled = false;
	bool SendPending = false;

	SPIClass* SpiInstance;
	RF24 Radio;

public:
	nRF24Transceiver(Scheduler& scheduler, SPIClass* spiInstance)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, ILoLaTransceiver()
		, SpiInstance(spiInstance)
		, Radio()
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
		if (!InterruptPending)
		{
			InterruptTimestamp = micros();
			InterruptPending = true;
			Task::enable();
		}
	}

	struct EventEnum
	{
		bool TxOk = false;
		bool TxFail = false;
		bool RxReady = false;
	} Event{};

public:
	virtual bool Callback() final
	{
		if (InterruptPending)
		{
			InterruptPending = false;

			// Get event source.
			Radio.whatHappened(Event.TxOk, Event.TxFail, Event.RxReady);

			if (Event.RxReady)
			{
				if (Radio.available())
				{
					const int8_t packetSize = Radio.getDynamicPayloadSize();

					if (packetSize >= LoLaPacketDefinition::MIN_PACKET_SIZE)
					{

						Radio.read(InBuffer, LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
						// Radio has no RSSI measure, all reception is perfect reception.
						// Rx Interrupt only occurs when packet has been fully received, the timestamp must be compensated.
						Listener->OnRx(InBuffer, InterruptTimestamp - GetRxDelay(packetSize), packetSize, UINT8_MAX);
					}
					else
					{
						// Invalid packet.
						//if (packetSize < 1)
						// Corrupt packet has been flushed.
						Listener->OnRxLost(InterruptTimestamp);
					}

					Radio.flush_rx();

					Task::enable();
					return true;
				}
				Radio.flush_rx();
			}

			if (Event.TxOk || Event.TxFail)
			{
				if (Event.TxFail)
				{
					//TODO: Log.
				}

				if (SendPending)
				{
					// Clear output FIFO.
					Radio.flush_tx();

					//TODO: verify if correct.
					//Radio.txStandBy();

					SendPending = false;
					Listener->OnTx();
				}
				else
				{
					// Unexpected interrupt.
				}
			}
		}

		// Risky sleep, if an interrupt is missed, radio will be stuck.
		//Task::disable();

		return false;
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
			// Set pin again, in case Transceiver has been reset.
			DisableInterrupt();
			pinMode(InterruptPin, INPUT);

			// Radio driver handles CePin and CsPin setup.
			if (!Radio.begin(SpiInstance, CePin, CsPin) && Radio.isChipConnected())
			{
				return false;
			}

			// TODO: Maximize SPI speed.
			//SpiInstance->setClockDivider();

			// Ensure the IC is present.
			if (!Radio.isChipConnected())
			{
				return false;
			}

			// LoLa already ensures integrity AND authenticity with its own MAC-CRC.
			Radio.setCRCLength(RF24_CRC_DISABLED);
			Radio.disableCRC();

			// LoLa doesn't specify any data rate, this should be defined in the user Link Protocol.
			if (Radio.setDataRate(DataRate))
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
			Radio.setAddressWidth(3);

			Radio.maskIRQ(true, true, true);

			// Open radio pipes.
			Radio.openReadingPipe(AddressPipe, BaseAddress);
			Radio.openWritingPipe(BaseAddress);


			// Initialize with low power level, let power manager adjust it after boot.
			Radio.setPALevel(RF24_PA_MIN);

			// Set random channel to start with.
			Radio.setChannel(GetRealChannel(UINT8_MAX / 2));

			EnableInterrupt();
			Task::enable();

			return true;
		}
		else
		{
			return false;
		}
	}

	virtual const bool Stop() final
	{
		DriverEnabled = false;
		//TODO: Stop Radio.

		Radio.closeReadingPipe(AddressPipe);
		Radio.stopListening();
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
		return DriverEnabled && !SendPending && !InterruptPending;
	}

	/// <summary>
	/// Packet copy to serial buffer, and start transmission.
	/// The first byte is the packet size, excluding the size byte.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		if (TxAvailable())
		{
			//TODO: Radio Tx.
			Radio.stopListening();
			Radio.setChannel(GetRealChannel(channel));

			// Sending clears all interrupt flags.
			// Wait for on Sent interrupt.
			SendPending = true;

			// Transmit packet.
			Radio.write(data, packetSize));

			Task::enable();

			return true;
		}

		return false;
	}

	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		//TODO: Clear tags and timestamps.
		//TODO: Skip if not needed.
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
		//TODO: Measure Tx duration value.
		return RadioCommsDelay;
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

	/// <summary>
	/// TODO: Measure and implement.
	/// 
	/// </summary>
	/// <param name="packetSize"></param>
	/// <returns></returns>
	const uint8_t GetRxDelay(const uint8_t packetSize)
	{
		switch (DataRate)
		{
		case RF24_250KBPS:
			return RadioCommsDelay + (packetSize * 8);
		case RF24_1MBPS:
			return RadioCommsDelay + (packetSize * 2);
		case RF24_2MBPS:
			return RadioCommsDelay + (packetSize * 1);
		default:
			return 0;
		}
	}


	void EnableInterrupt()
	{
		attachInterrupt(digitalPinToInterrupt(InterruptPin), OnInterrupt, FALLING);
	}

	void DisableInterrupt()
	{
		detachInterrupt(digitalPinToInterrupt(InterruptPin));
	}
};
#endif