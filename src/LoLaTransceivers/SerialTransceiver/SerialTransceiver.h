// SerialTransceiver.h
// For use with serial/UART.

#ifndef _LOLA_SERIAL_TRANSCEIVER_h
#define _LOLA_SERIAL_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>

/// <summary>
/// Serial/UART LoLa Packet Driver.
/// </summary>
/// <typeparam name="SerialType"></typeparam>
/// <typeparam name="BaudRate"></typeparam>
/// <typeparam name="RxInterruptPin"></typeparam>
/// <typeparam name="RxBufferSize"></typeparam>
/// <typeparam name="TxBufferSize"></typeparam>
template<typename SerialType,
	const uint32_t BaudRate,
	const uint8_t RxInterruptPin,
	const size_t RxBufferSize = 64,
	const size_t TxBufferSize = 64>
class SerialTransceiver : private Task, public virtual ILoLaTransceiver
{
private:
#if defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_ESP32)
	static constexpr uint32_t TransmitDelayMicros = 3;
#else
	static constexpr uint32_t TransmitDelayMicros = 6;
#endif
	static constexpr uint32_t TxSeparationPauseMicros = 100;

	static constexpr uint32_t ReferenceLong = 1645000;
	static constexpr uint32_t ReferenceShort = 45000;
	static constexpr uint8_t ReferenceBaudrate = 200;

	static constexpr uint16_t DurationShort = (uint16_t)((ReferenceShort * ReferenceBaudrate) / BaudRate);
	static constexpr uint16_t DurationLong = (uint16_t)((ReferenceLong * ReferenceBaudrate) / BaudRate);
	static constexpr uint16_t DurationRange = DurationLong - DurationShort;

private:
	enum TxStateEnum
	{
		NoTx,
		TxStart,
		TxEnd
	};

	enum RxStateEnum
	{
		NoRx,
		RxStart,
		RxEnd
	};

private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE] {};

	ILoLaTransceiverListener* Listener = nullptr;

	SerialType* IO;

private:
	volatile uint32_t RxStartTimestamp = 0;
	uint32_t TxStartTimestamp = 0;
	uint32_t TxEndTimestamp = 0;

	volatile RxStateEnum RxState = RxStateEnum::NoRx;
	TxStateEnum TxState = TxStateEnum::NoTx;

	uint8_t RxIndex = 0;
	uint8_t RxSize = 0;
	uint8_t TxSize = 0;

private:
	void (*OnRxInterrupt)(void) = nullptr;
	bool DriverEnabled = false;

public:
	SerialTransceiver(Scheduler& scheduler, SerialType* io)
		: ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, IO(io)
	{
#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, LOW);
		pinMode(RX_TEST_PIN, OUTPUT);
#endif

#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, LOW);
		pinMode(TX_TEST_PIN, OUTPUT);
#endif
	}

	void SetupInterrupt(void (*onRxInterrupt)(void))
	{
		OnRxInterrupt = onRxInterrupt;
	}

	/// <summary>
	/// To be called on serial receive interrupt.
	/// </summary>
	void OnSeriaInterrupt()
	{
#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, HIGH);
		digitalWrite(RX_TEST_PIN, LOW);
#endif

		// Only one interrupt expected for each packet.
		DisableInterrupt();

		if (RxState == RxStateEnum::NoRx)
		{
			RxStartTimestamp = micros();
			RxState = RxStateEnum::RxStart;
			Task::enable();
		}
	}

public:
	virtual bool Callback() final
	{
		if (DriverEnabled)
		{
			switch (TxState)
			{
			case TxStateEnum::NoTx:
				break;
			case TxStateEnum::TxStart:
				// Wait for Tx buffer to be completely free.
				if (((micros() - TxStartTimestamp) > GetTimeToAir(TxSize)) &&
					(IO->availableForWrite() >= (TxBufferSize - 1)))
				{
					TxEndTimestamp = micros();
					TxState = TxStateEnum::TxEnd;
				}
				break;
			case TxStateEnum::TxEnd:
				// TxSeparationPauseMicros at the end, to clearly separate packets.
				if (micros() - TxStartTimestamp > (GetTimeToAir(TxSize) + GetDurationInAir(TxSize)))
				{
					Listener->OnTx();
					TxState = TxStateEnum::NoTx;
				}
				break;
			default:
				break;
			}

			switch (RxState)
			{
			case RxStateEnum::NoRx:
				break;
			case RxStateEnum::RxStart:
				if (micros() - RxStartTimestamp > GetDurationInAir(1))
				{
					if (IO->available())
					{
						// First byte is the packet size.
						RxSize = IO->read();
						if (RxSize >= LoLaPacketDefinition::MIN_PACKET_SIZE
							&& RxSize <= LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
						{
							RxIndex = 0;
							RxState = RxStateEnum::RxEnd;
						}
						else
						{
							ClearRx();
						}
					}
					else
					{
						// Interrupt fired but no data was found on serial buffer after the wait period.
						ClearRx();
					}
				}
				break;
			case RxStateEnum::RxEnd:
				// Wait for Rx buffer to be have RxSize.
				while (IO->available() > 0 && RxIndex < RxSize)
				{
					InBuffer[RxIndex] = IO->read();
					RxIndex++;
				}

				if (RxIndex >= RxSize)
				{
					Listener->OnRx(InBuffer, RxStartTimestamp, RxSize, UINT8_MAX);
					ClearRx();
				}
				else if (micros() - RxStartTimestamp > DurationLong)
				{
					Listener->OnRxLost(RxStartTimestamp);
					ClearRx();
				}
				break;
			default:
				break;
			}

			if (TxState != TxStateEnum::NoTx || RxState != RxStateEnum::NoRx)
			{
				Task::enable();
				return true;
			}
		}

		Task::disable();
		return false;
	}

	// ILoLaTransceiver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr;
	}

	virtual const bool Start() final
	{
		if (IO != nullptr
			&& Listener != nullptr
			&& OnRxInterrupt != nullptr)
		{
			pinMode(RxInterruptPin, INPUT_PULLUP);

			IO->begin(BaudRate);
			IO->clearWriteError();
			IO->flush();


			TxState = TxStateEnum::NoTx;
			DriverEnabled = true;

			ClearRx();

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
		IO->end();
		IO->clearWriteError();
		IO->flush();
		Task::disable();

		return true;
	}

	/// <summary>
	/// Full Duplex Serial isn't limited by Rx, only denies Tx if already in Tx.
	/// </summary>
	/// <returns></returns>
	virtual const bool TxAvailable() final
	{
		return DriverEnabled && TxState == TxStateEnum::NoTx;
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
#ifdef TX_TEST_PIN
			digitalWrite(TX_TEST_PIN, HIGH);
			digitalWrite(TX_TEST_PIN, LOW);
#endif
			TxStartTimestamp = micros();
			if (IO->write(&packetSize, 1) == 1
				&& IO->write(data, packetSize) == packetSize)
			{
				TxSize = packetSize;
				TxState = TxStateEnum::TxStart;
				Task::enable();

				return true;
			}
		}

		return false;
	}

	/// <summary>
	/// Serial is always in Rx mode.
	/// </summary>
	/// <param name="channel"></param>
	virtual void Rx(const uint8_t channel) final
	{
		// Notify forced TX end.
		if (TxState != TxStateEnum::NoTx)
		{
			Serial.println("Rx killed Tx.");
			Listener->OnTx();
		}

		TxState = TxStateEnum::NoTx;
	}

	/// <summary>
	/// Serial will start outputing as soon as the first byte is pushed.
	/// The fixed delay is mostly hardware dependent and small (<10us).
	/// </summary>
	/// <param name="packetSize">Number of bytes in the packet.</param>
	/// <returns>The expected transmission duration in microseconds.</returns>
	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return TransmitDelayMicros;
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		// +1 packet size for Size byte.
		return (uint32_t)DurationShort + (((uint32_t)DurationRange * (packetSize + 1)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	}

	virtual const uint16_t GetTimeToHop() final
	{
		return 1;
	}

private:
	void EnableInterrupt()
	{
		attachInterrupt(digitalPinToInterrupt(RxInterruptPin), OnRxInterrupt, FALLING);
		interrupts();
	}

	void DisableInterrupt()
	{
		detachInterrupt(digitalPinToInterrupt(RxInterruptPin));
	}

	void ClearRx()
	{
		// Clear input buffer and size.
		RxSize = 0;
		while (IO->available())
		{
			IO->read();
		}

		RxState = RxStateEnum::RxStart;
		// Force pending interrupts and enable for next.
		EnableInterrupt();

		RxState = RxStateEnum::NoRx;
		Task::enable();
	}
};
#endif