// SerialLoLaPacketDriver.h
// For use with serial/UART.

#ifndef _LOLA_SERIAL_PACKET_DRIVER_h
#define _LOLA_SERIAL_PACKET_DRIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaPacketDriver.h>


/// <summary>
/// Serial/UART LoLa Packet Driver.
/// 
/// </summary>
template<const uint8_t MaxPacketSize,
	typename SerialType,
	const uint32_t BaudRate = 9600>
class SerialLoLaPacketDriver : private Task, public virtual ILoLaPacketDriver
{
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
		if (consteval(IsNotZero<byteDurationMicros>()))
		{
			return byteDurationMicros / 1000;
		}
		else
		{
			return 1;
		}
	}

	// Estimate byte duration with baud-rate.
	static const uint32_t ByteDurationMicros = (1000000L * 8) / (BaudRate);
	static const uint32_t ByteDurationMillis = GetByteDurationMillis<ByteDurationMicros>();

	static const uint32_t PacketSeparationPauseMicros = 5000000L / BaudRate;

	ILoLaPacketDriverListener* PacketListener = nullptr;

	SerialType* IO;

	uint8_t InBuffer[MaxPacketSize];

	uint32_t ReceiveTimestamp = 0;
	uint32_t LastReceivedTimestamp = 0;

	uint32_t SendEndTimestamp = 0;


	bool DriverEnabled = false;
	bool SendPending = false;
	bool ReceivePending = false;

public:
	SerialLoLaPacketDriver(Scheduler& scheduler, SerialType* io)
		: Task(0, TASK_FOREVER, &scheduler, false)
		, ILoLaPacketDriver()
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
					PacketListener->OnSent(true);
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
							PacketListener->OnReceived(InBuffer, ReceiveTimestamp, size, UINT8_MAX);
						}
						else
						{
							PacketListener->OnReceiveLost(ReceiveTimestamp);
						}
					}
					else
					{
						ReceivePending = false;
						PacketListener->OnReceiveLost(ReceiveTimestamp);
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
	virtual const bool DriverSetup(ILoLaPacketDriverListener* packetListener)
	{
		DriverStop();

		PacketListener = packetListener;

		return PacketListener != nullptr;
	}

	virtual const bool DriverStart() final
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

	virtual const bool DriverStop() final
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
	virtual const bool DriverCanTransmit() final
	{
		return DriverEnabled && !SendPending && IO->availableForWrite() && ((micros() - SendEndTimestamp) > PacketSeparationPauseMicros);
	}

	/// <summary>
	/// Packet it copied to serial buffer, freeing the packet buffer.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	virtual const bool DriverTransmit(uint8_t* data, const uint8_t length) final
	{
		if (IO->write(data, length) == length)
		{
			SendEndTimestamp = micros() + (ByteDurationMicros * (length - 1));
			SendPending = true;

			return true;
		}

		return false;
	}

	virtual const uint8_t GetLastRssiIndicator() final
	{
		// All reception is perfect reception.
		return UINT8_MAX;
	}

	virtual const uint32_t GetTransmitTimeoutMicros(const uint8_t packetSize) final
	{
		// Estimate byte duration with baud-rate.
		return ByteDurationMicros * (packetSize + 1);
	}

	virtual const uint32_t GetReplyTimeoutMicros(const uint8_t packetSize) final
	{
		// A bit longe than expected, just for margin.
		return GetTransmitTimeoutMicros(packetSize + 3);
	}
};
#endif