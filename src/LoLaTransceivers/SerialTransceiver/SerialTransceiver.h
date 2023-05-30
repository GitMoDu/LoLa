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
	static constexpr uint32_t TransmitDelayMicros = 10;
#else
	static constexpr uint32_t TransmitDelayMicros = 25;
#endif

	// TODO: Baudrate aware.
	static constexpr uint32_t TxSeparationPauseMicros = 200;
	static constexpr uint32_t RxWaitPauseMicros = 150;

	// TODO: Baudrate and size aware?
	static constexpr uint32_t RxTimeoutMicros = 5000;


private:
	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE];

	ILoLaTransceiverListener* Listener = nullptr;

	SerialType* IO;
	uint32_t ReceiveTimestamp = 0;


private:
	uint32_t TxEndTimestamp = 0;

	enum TxStateEnum
	{
		NoTx,
		TxStart,
		TxEnd
	};

	TxStateEnum TxState = TxStateEnum::NoTx;

private:
	enum RxStateEnum
	{
		NoRx,
		RxStart,
		RxEnd
	};
	volatile uint32_t RxStartTimestamp = 0;

	volatile RxStateEnum RxState = RxStateEnum::NoRx;
	uint8_t RxIndex = 0;
	uint8_t RxSize = 0;


private:
	bool DriverEnabled = false;

public:
	SerialTransceiver(Scheduler& scheduler, SerialType* io)
		: Task(0, TASK_FOREVER, &scheduler, false)
		, ILoLaTransceiver()
		, IO(io)
	{}

	void (*OnRxInterrupt)(void) = nullptr;

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
				if (IO->availableForWrite() >= TxBufferSize - 1)
				{
					TxEndTimestamp = micros();
					TxState = TxStateEnum::TxEnd;
				}
				break;
			case TxStateEnum::TxEnd:
				// Brief pause at the end, to clearly separate packets.
				if (micros() - TxEndTimestamp > TxSeparationPauseMicros)
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
				if (micros() - RxStartTimestamp > RxWaitPauseMicros && IO->available() > 0)
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
						Listener->OnRxLost(RxStartTimestamp);
						ClearRx();
					}
				}
				else if (micros() - RxStartTimestamp > RxTimeoutMicros)
				{
					Listener->OnRxLost(RxStartTimestamp);
					ClearRx();
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
					if (RxSize >= LoLaPacketDefinition::MIN_PACKET_SIZE)
					{
						Listener->OnRx(InBuffer, RxStartTimestamp, RxSize, UINT8_MAX);
					}
					ClearRx();
				}
				else if (micros() - RxStartTimestamp > RxTimeoutMicros)
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

	// ILoLaPacketDriver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener)
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
			pinMode(RxInterruptPin, INPUT);

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
		if (TxAvailable()
			&& IO->write(&packetSize, 1) == 1
			&& IO->write(data, packetSize) == packetSize
			)
		{
			Serial.print(F("Tx ("));
			Serial.print(packetSize);
			Serial.println(F(") bytes."));

			TxState = TxStateEnum::TxStart;
			Task::enable();

			return true;
		}

		return false;
	}

	/// <summary>
	/// Serial is always in Rx mode.
	/// </summary>
	/// <param name="channel"></param>
	virtual void Rx(const uint8_t channel) final
	{}

	/// <summary>
	/// Serial will start outputing as soon as the first byte is pushed.
	/// The fixed delay is mostly hardware dependent and small (<10us).
	/// </summary>
	/// <param name="packetSize">Number of bytes in the packet.</param>
	/// <returns>The expected transmission duration in microseconds.</returns>
	virtual const uint32_t GetTransmitDurationMicros(const uint8_t packetSize) final
	{
		return TransmitDelayMicros;
	}

private:
	void EnableInterrupt()
	{
		attachInterrupt(digitalPinToInterrupt(RxInterruptPin), OnRxInterrupt, FALLING);
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

		// Force pending interrupts and enabled for next.
		RxState = RxStateEnum::RxStart;
		EnableInterrupt();
		interrupts();

		RxState = RxStateEnum::NoRx;
		EnableInterrupt();
	}
};
#endif