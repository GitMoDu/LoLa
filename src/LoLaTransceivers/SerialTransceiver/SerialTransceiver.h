// SerialTransceiver.h
// For use with serial/UART.

#ifndef _LOLA_SERIAL_TRANSCEIVER_h
#define _LOLA_SERIAL_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>


/// <summary>
/// Serial/UART LoLa Packet Driver.
/// 
/// </summary>
template<const uint8_t InterruptPin,
	typename SerialType,
	const uint32_t BaudRate>
class SerialTransceiver : private Task, public virtual ILoLaTransceiver
{
	template<const uint8_t InterruptPin,
		typename SerialType,
		const uint32_t BaudRate>
	class TimestampedSerial
	{
	private:
		SerialType& SerialInstance;

		volatile uint32_t RxStartTimestamp = 0;

		volatile bool RxPending = false;

		volatile bool RxActive = false;

	public:
		TimestampedSerial(SerialType& serialInstance)
			: SerialInstance(serialInstance)
		{

		}

		void StopRx()
		{
			if (RxActive)
			{
				RxActive = false;
				ClearRx();
			}
		}

		void StartRx()
		{
			if (!RxActive)
			{
				RxActive = true;
				ClearRx();
			}
		}

		const bool Setup(void* onInterruptFunction())
		{
			if (onInterruptFunction != nullptr) 
			{
				return true;
			}

			return false;
		}

		void OnPinInterrupt()
		{
			// Only one interrupt for each packet.
			DisableInterrupt();

			if (RxActive)
			{
				// Flag pending.
				RxPending = true;

				// Store timestamp for notification.
				RxStartTimestamp = micros();
			}
		}

	private:
		void EnableInterrupt()
		{
			//TODO: Implement. InterruptPin
		}
		void DisableInterrupt()
		{
			//TODO: Implement. InterruptPin
		}
		void ClearRx()
		{
			RxPending = false;

			//TODO: Clear serial input buffer.
		}
	};
private:
	// Compile time static definition.
	template<const uint32_t byteDurationMicros>
	static constexpr bool IsNotZero()
	{
		return (byteDurationMicros / 1000L) > 0;
	}

	template<const uint32_t byteDurationMicros>
	static constexpr uint32_t GetByteDurationMillis()
	{
		return byteDurationMicros / 1000;
		/*if (consteval(IsNotZero<byteDurationMicros>()))
		{
			return byteDurationMicros / 1000;
		}
		else
		{
			return 1;
		}*/
	}

	// Estimate byte duration with baud-rate.
	static const uint32_t ByteDurationMicros = (1000000L * 8) / (BaudRate);
	static const uint32_t ByteDurationMillis = GetByteDurationMillis<ByteDurationMicros>();

	static const uint32_t PacketSeparationPauseMicros = 5000000L / BaudRate;

	ILoLaTransceiverListener* Listener = nullptr;

	SerialType* IO;

	uint8_t InBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE];

	uint32_t ReceiveTimestamp = 0;
	uint32_t LastReceivedTimestamp = 0;

	uint32_t SendEndTimestamp = 0;


	bool DriverEnabled = false;
	bool SendPending = false;
	bool ReceivePending = false;

public:
	SerialTransceiver(Scheduler& scheduler, SerialType* io)
		: Task(0, TASK_FOREVER, &scheduler, false)
		, ILoLaTransceiver()
		, IO(io)
	{}

	/// <summary>
	/// To be called on serial receive interrupt.
	/// </summary>
	void OnSerialEvent()
	{
		LastReceivedTimestamp = micros();
		if (!ReceivePending)
		{
			ReceiveTimestamp = LastReceivedTimestamp;
			ReceivePending = true;
		}

		Task::enable();
	}

public:
	virtual bool Callback() final
	{
		if (DriverEnabled)
		{
			if (SendPending)
			{
				if (micros() > SendEndTimestamp)
				{
					Listener->OnTx();
					SendPending = false;
				}
			}
			else if (ReceivePending)
			{
				// For every interrupt, processing is delayed by PacketSeparationPauseMicros.
				if ((micros() - LastReceivedTimestamp) > PacketSeparationPauseMicros)
				{
					const uint8_t pending = Serial.available();
					if (pending > 0)
					{
						uint8_t size = Serial.readBytes((char*)InBuffer, pending);

						// Clear flag as soon as the packet has been read off serial buffer.
						ReceivePending = false;

						if (size > 0)
						{
							// All reception is perfect reception.
							Listener->OnRx(InBuffer, ReceiveTimestamp, size, UINT8_MAX);
						}
						else
						{
							Listener->OnRxLost(ReceiveTimestamp);
						}
					}
					else
					{
						ReceivePending = false;
						Listener->OnRxLost(ReceiveTimestamp);
					}
				}
				Task::enable();
			}
			else
			{
				Task::disable();
			}
		}
		else
		{
			Task::disable();
		}
	}

	// ILoLaPacketDriver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener)
	{
		Stop();

		Listener = listener;

		return Listener != nullptr;
	}

	virtual const bool Start() final
	{
		if (IO != nullptr)
		{
			ReceivePending = false;
			DriverEnabled = true;
			IO->begin(BaudRate);
			IO->clearWriteError();
			IO->flush();
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
		ReceivePending = false;
		DriverEnabled = false;
		IO->end();
		IO->clearWriteError();
		IO->flush();
		Task::disable();

		return true;
	}

	/// <summary>
	/// Serial isn't limited by receiving, only block if busy sending.
	/// </summary>
	/// <returns></returns>
	virtual const bool TxAvailable() final
	{
		return DriverEnabled && !SendPending && IO->availableForWrite() && ((micros() - SendEndTimestamp) > PacketSeparationPauseMicros);
	}

	/// <summary>
	/// Packet copy to serial buffer, and start transmission.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	virtual const bool Tx(uint8_t* data, const uint8_t length) final
	{
		if (IO->write(data, length) == length)
		{
			SendEndTimestamp = micros() + (ByteDurationMicros * (length - 1));
			SendPending = true;

			return true;
		}

		return false;
	}

	/// <summary>
	/// Estimate duration with baud-rate.
	/// Packet size is not a factor, as serial will start outputing
	///  as soon as the first byte is pushed.
	/// </summary>
	/// <param name="packetSize">Number of bytes in the packet.</param>
	/// <returns>The expected transmission duration in microseconds.</returns>
	virtual const uint32_t GetTransmitDurationMicros(const uint8_t packetSize) final
	{
		return ByteDurationMicros;
	}
};
#endif