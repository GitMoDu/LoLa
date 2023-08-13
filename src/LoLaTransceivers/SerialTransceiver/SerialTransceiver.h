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
	static constexpr uint32_t ReferenceLong = 33075;
	static constexpr uint32_t ReferenceShort = 6100;
	static constexpr uint16_t ReferenceBaudrate = 9600;
	static constexpr uint16_t MinBaudrate = 80000;

	static constexpr uint16_t DurationShort = (uint16_t)((ReferenceShort * ReferenceBaudrate) / BaudRate);
	static constexpr uint16_t DurationLong = (uint16_t)((ReferenceLong * ReferenceBaudrate) / BaudRate);
	static constexpr uint16_t DurationRange = DurationLong - DurationShort;

	static constexpr uint32_t ByteDuration = (DurationLong / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	static constexpr uint16_t BitDuration = (DurationLong / ((uint16_t)LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE * 8));
	static constexpr uint16_t TxHoldDuration = 10 + (BitDuration * 2);

	static constexpr uint32_t LineDuration = BitDuration;

private:
	SerialType* IO;

	ILoLaTransceiverListener* Listener = nullptr;

	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	volatile uint32_t RxStartTimestamp = 0;
	uint32_t RxEndTimestamp = 0;
	uint32_t TxStartTimestamp = 0;

	volatile RxStateEnum RxState = RxStateEnum::NoRx;
	TxStateEnum TxState = TxStateEnum::NoTx;

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
		// Only one interrupt expected for each packet.
		DisableInterrupt();

		if (RxState == RxStateEnum::NoRx)
		{
#ifdef RX_TEST_PIN
			digitalWrite(RX_TEST_PIN, HIGH);
#endif
			RxStartTimestamp = micros();
			RxState = RxStateEnum::RxStart;
		}

		Task::enable();
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
				// Wait for Tx buffer to be free after the minimum duration.
				if ((micros() - TxStartTimestamp > LineDuration + ByteDuration)
					&& (IO->availableForWrite() >= (TxBufferSize - 1)))
				{
					TxState = TxStateEnum::TxEnd;
				}
				break;
			case TxStateEnum::TxEnd:
				// Added pause at the end, to clearly separate packets.
				if (micros() - TxStartTimestamp > LineDuration + GetDurationInAir(TxSize) + TxHoldDuration)
				{
#ifdef TX_TEST_PIN
					digitalWrite(TX_TEST_PIN, LOW);
#endif
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
				// Wait for at least the base transmit and 1 byte period.
				if (micros() - RxStartTimestamp > LineDuration + ByteDuration)
				{
					RxSize = 0;
					RxEndTimestamp = micros();
					RxState = RxStateEnum::RxEnd;
				}
				break;
			case RxStateEnum::RxEnd:
				if (IO->available())
				{
					while (IO->available())
					{
						InBuffer[RxSize] = IO->read();
						RxSize++;

						// Packet full, don't wait more.
						if (RxSize == LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
						{
							OnRxDone(true);
						}
						else if (RxSize > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
						{
							OnRxDone(false);
						}
					}
					RxEndTimestamp = micros();
				}
				else if (micros() - RxEndTimestamp > ByteDuration)
				{
					if (RxSize >= LoLaPacketDefinition::MIN_PACKET_SIZE && RxSize <= LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
					{
						OnRxDone(true);
					}
					else
					{
						OnRxDone(false); // Invalid size packet, discard.
					}
				}
				else if (micros() - RxStartTimestamp > DurationLong + ByteDuration)
				{
					OnRxDone(false); // Packet timeout, discard.
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
			&& OnRxInterrupt != nullptr
			&& BaudRate >= MinBaudrate)
		{
			pinMode(RxInterruptPin, INPUT_PULLUP);

			IO->begin(BaudRate);
			IO->clearWriteError();
			IO->flush();


			TxState = TxStateEnum::NoTx;
			ClearRx();
			DriverEnabled = true;
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
	/// </summary>
	/// <param name="data"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
	{
		if (TxAvailable())
		{
#ifdef TX_TEST_PIN
			digitalWrite(TX_TEST_PIN, HIGH);
#endif
			if (IO->write(data, packetSize) == packetSize)
			{
				TxStartTimestamp = micros();
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
		Task::enableIfNot();
	}

	/// <summary>
	/// Serial will start outputing as soon as the first byte is pushed.
	/// The fixed delay is mostly hardware dependent and small (<10us).
	/// </summary>
	/// <param name="packetSize">Number of bytes in the packet.</param>
	/// <returns>The expected transmission duration in microseconds.</returns>
	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return LineDuration;
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		if (packetSize >= LoLaPacketDefinition::MIN_PACKET_SIZE)
		{
			return (uint32_t)DurationShort
				+ (((uint32_t)DurationRange * (packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE))
					/ (LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE - LoLaPacketDefinition::MIN_PACKET_SIZE));
		}
		else
		{
			return DurationShort;
		}
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
		// Clear input buffer.
		while (IO->available())
		{
			IO->read();
		}
		// Force pending interrupts and enable for next.
		RxState = RxStateEnum::RxStart;
		RxSize = 0;
		EnableInterrupt();

		RxState = RxStateEnum::NoRx;
		Task::enable();
	}

	void OnRxDone(const bool rxGood)
	{
#ifdef RX_TEST_PIN
		digitalWrite(RX_TEST_PIN, LOW);
#endif
		if (rxGood)
		{
			Listener->OnRx(InBuffer, RxStartTimestamp, RxSize, UINT8_MAX);
		}
		else
		{
			Listener->OnRxLost(RxStartTimestamp);
		}

		ClearRx();
	}
};
#endif