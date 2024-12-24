// UartTransceiver.h

#ifndef _LOLA_UART_TRANSCEIVER_h
#define _LOLA_UART_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include "UartAsyncReceiver.h"
#include "CobsCodec.h"

#include "../ILoLaTransceiver.h"


/// <summary>
/// UART/Serial LoLa Transceiver.
/// Packets are framed using Consistent Overhead Byte Stuffing (COBS).
/// The encoded data consists only of bytes with values from 0x01 to 0xFF.
/// The 0x00 byte is be used to indicate packet start/end.
/// Detects malformed input data.
/// Rx start detection timestamping relies on interrupt tapping of the Rx pin.
/// This works on any Arduino core, without replacing the Serial interrupt handling.
/// </summary>
/// <typeparam name="SerialType">Type definition of HardwareSerial.</typeparam>
/// <typeparam name="BaudRate">At least MIN_BAUD_RATE.</typeparam>
/// <typeparam name="RxInterruptPin">The pin tied to UART Rx, for start detection and timestamping.</typeparam>
template<typename SerialType,
	const uint8_t RxInterruptPin,
	const uint32_t BaudRate = 115200>
class UartTransceiver final : public virtual ILoLaTransceiver, public virtual IUartListener, private TS::Task
{
private:
	static constexpr uint16_t TRANSCEIVER_ID = 0xC0B5;

private:
	enum class TxStateEnum : uint8_t
	{
		Disabled,
		Clear,
		Ready,
		TxStart,
		TxEnd
	};

private:
	static constexpr uint8_t Delimiter = 0;

	static constexpr uint8_t COBS_OVERHEAD = 1;
	static constexpr uint8_t MAX_COBS_SIZE = LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE + COBS_OVERHEAD;

	static constexpr uint8_t GetFrameSize(const uint8_t packetSize)
	{
		return packetSize + COBS_OVERHEAD + 2;
	}

	static constexpr uint8_t MAX_FRAME_SIZE = GetFrameSize(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);

private:
	static constexpr uint32_t MIN_BAUD_RATE = 80000;
	static constexpr uint32_t REFERENCE_LONG = 35308;
	static constexpr uint32_t REFERENCE_SHORT = 8230;
	static constexpr uint16_t REFERENCE_BAUD_RATE = 9600;

private:
	static constexpr uint16_t DurationShort = (uint32_t)((REFERENCE_SHORT * REFERENCE_BAUD_RATE) / BaudRate);
	static constexpr uint16_t DurationLong = (uint32_t)((REFERENCE_LONG * REFERENCE_BAUD_RATE) / BaudRate);
	static constexpr uint16_t DurationRange = DurationLong - DurationShort;

	static constexpr uint16_t ByteDuration = (DurationLong / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	static constexpr uint16_t BitDuration = (DurationLong / ((uint16_t)LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE * 8));
	static constexpr uint16_t LineDuration = BitDuration;

private:
	// Don't hog the loop for more than a few us at a time.
	static constexpr uint32_t MaxHoldMicros = 100;

	// Wait for the UART driver to catch up.
	static constexpr uint32_t RxWaitMicros = 50 + ByteDuration;

	static constexpr uint32_t RxPauseMicros = 50 + (ByteDuration * 2);
	static constexpr uint32_t TxPauseMicros = 150 + (ByteDuration * 2);
	static constexpr uint32_t RxTimeoutMicros = DurationLong + TxPauseMicros;

	static constexpr uint8_t CalibrationSamples = 50;

	uint16_t EncodeBaseDuration = 1;
	uint16_t EncodeRangeDuration = 1;

	uint8_t BufferSize = 0;

private:
	SerialType& IO;

private:
	UartAsyncReceiver<SerialType, RxInterruptPin, MAX_COBS_SIZE, RxTimeoutMicros, RxPauseMicros, RxWaitMicros, MaxHoldMicros> Receiver;

	ILoLaTransceiverListener* Listener = nullptr;

	uint8_t RxBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};
	uint8_t TxBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	uint32_t TxTimestamp = 0;
	TxStateEnum TxState = TxStateEnum::Disabled;

public:
	UartTransceiver(TS::Scheduler& scheduler, SerialType& io)
		: ILoLaTransceiver()
		, IUartListener()
		, TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, IO(io)
		, Receiver(scheduler, io, *this)
	{
	}

	void SetupInterrupt(void (*rxInterrupt)(void))
	{
		Receiver.SetupInterrupt(rxInterrupt);
	}

	void OnInterrupt()
	{
		Receiver.OnRxInterrupt();
	}

public:
	virtual void OnUartRx(const uint32_t timestamp, const uint8_t* data, const uint8_t size) final
	{
		const uint_fast8_t rxSize = CobsCodec::Decode(data, RxBuffer, size);

		if (rxSize < LoLaPacketDefinition::MIN_PACKET_SIZE)
		{
			//Serial.println(F("Rx Invalid size packet, discard."));
		}
		else if (rxSize > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
		{
			//Serial.println(F("Rx Overflow detected."));
		}
		else
		{
			Listener->OnRx(RxBuffer, timestamp, rxSize, UINT8_MAX);
		}
	}

	virtual bool Callback() final
	{
		switch (TxState)
		{
		case TxStateEnum::Clear:
			if (IO.availableForWrite() >= BufferSize)
			{
				TxState = TxStateEnum::Ready;
				TS::Task::disable();
			}
			break;
		case TxStateEnum::TxStart:
			if (IO.availableForWrite() >= BufferSize)
			{
				TxState = TxStateEnum::TxEnd;
				TxTimestamp = micros();
			}
			break;
		case TxStateEnum::TxEnd:
			// Wait for Tx buffer to be free after the minimum duration.
			if (micros() - TxTimestamp > TxPauseMicros)
			{
				Listener->OnTx();
				TxState = TxStateEnum::Ready;
				TS::Task::disable();
			}
			break;
		case TxStateEnum::Disabled:
		case TxStateEnum::Ready:
		default:
			TS::Task::disable();
			break;
		}

		return false;
	}

public:// ILoLaTransceiver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr && Calibrate();
	}

	virtual const bool Start() final
	{
		if (Listener != nullptr
			&& BaudRate >= MIN_BAUD_RATE)
		{
			if (Receiver.Start())
			{
				IO.begin(BaudRate);
				IO.clearWriteError();
				BufferSize = IO.availableForWrite();

				if (BufferSize < MAX_FRAME_SIZE)
				{
					IO.end();
					return false;
				}

				TxState = TxStateEnum::Clear;
				TS::Task::enable();

				return true;
			}
		}

		return false;
	}

	virtual const bool Stop() final
	{
		Receiver.Stop();
		TxState = TxStateEnum::Disabled;
		TS::Task::disable();

		return true;
	}

	/// <summary>
	/// UART isn't limited by Rx.
	/// Tx limited by pause after last Tx ended.
	/// </summary>
	/// <returns></returns>
	virtual const bool TxAvailable() final
	{
		return TxState == TxStateEnum::Ready;
	}

	/// <summary>
	/// Packet copy to serial buffer, and start transmission.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="packetSize"></param>
	/// <param name="channel"></param>
	/// <returns></returns>
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
	{
		if (TxState == TxStateEnum::Ready)
		{
			const uint8_t txSize = CobsCodec::Encode(data, TxBuffer, packetSize);

			if (txSize > 0)
			{
				if (IO.write(Delimiter)
					&& IO.write(TxBuffer, txSize) == txSize
					&& IO.write(Delimiter))
				{
					TxState = TxStateEnum::TxStart;
					TS::Task::enable();
					TxTimestamp = micros();

					return true;
				}
				else
				{
					TxState = TxStateEnum::Clear;
					TS::Task::enable();
				}
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
	}

	virtual const uint32_t GetTransceiverCode() final
	{
		return (uint32_t)TRANSCEIVER_ID
			| (uint32_t)(BaudRate / 100) << 16;
	}

	/// <summary>
	/// Serial will start outputing as soon as the first byte is pushed.
	/// The fixed delay is mostly hardware dependent and small (<10us).
	/// </summary>
	/// <param name="packetSize">Number of bytes in the packet.</param>
	/// <returns>The expected transmission duration in microseconds.</returns>
	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		if (packetSize >= LoLaPacketDefinition::MIN_PACKET_SIZE)
		{
			return ((uint32_t)EncodeBaseDuration)
				+ ((((uint32_t)EncodeRangeDuration) * (packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE))
					/ (LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE - LoLaPacketDefinition::MIN_PACKET_SIZE));
		}
		else
		{
			return LineDuration + EncodeBaseDuration;
		}
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
	const bool Calibrate()
	{
		uint32_t start, end, duration;
		volatile uint8_t benchIndex = 0;

#if defined(DEBUG_LOLA)
		if (!UnitTest())
		{
			return false;
		}
#endif
		const uint32_t calibrationStart = micros();

		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE; i++)
		{
			RxBuffer[i] = i;
		}

		start = micros();
		for (uint_fast8_t i = 0; i < CalibrationSamples; i++)
		{
			benchIndex = CobsCodec::Encode(RxBuffer, TxBuffer, LoLaPacketDefinition::MIN_PACKET_SIZE) & TxAvailable();
		}
		end = micros();
		duration = (end - start) / CalibrationSamples;
		if (duration > 0)
		{
			if (duration <= INT16_MAX)
			{
				EncodeBaseDuration = duration;
			}
			else
			{
				return false;
			}
		}
		else
		{
			EncodeBaseDuration = 1;
		}

		start = micros();
		for (uint_fast8_t i = 0; i < CalibrationSamples; i++)
		{
			benchIndex = CobsCodec::Encode(RxBuffer, TxBuffer, LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE) & TxAvailable();
		}
		end = micros();
		duration = (end - start) / CalibrationSamples;
		if (duration > EncodeBaseDuration)
		{
			if (duration - EncodeBaseDuration <= INT16_MAX)
			{
				EncodeRangeDuration = duration - EncodeBaseDuration;
			}
			else
			{
				return false;
			}
		}
		else
		{
			EncodeRangeDuration = 1;
		}

		if (benchIndex != 0)
		{
			return false;
		}

#if defined(DEBUG_LOLA)
		const uint32_t calibrationEnd = micros();
		Serial.print(F("UART @ "));
		Serial.print(BaudRate);
		Serial.println(F(" bps"));
		Serial.print(F("\tCalibration took "));
		Serial.print(calibrationEnd - calibrationStart);
		Serial.println(F(" us"));
		Serial.print(F("\tEncode short: "));
		Serial.print(EncodeBaseDuration);
		Serial.println(F("us"));
		Serial.print(F("\tEncode long: "));
		Serial.print(EncodeBaseDuration + EncodeRangeDuration);
		Serial.println(F("us"));
#endif

		return true;
	}

#if defined(DEBUG_LOLA)
private:
	const bool UnitTest()
	{
		uint8_t cobsBuffer[MAX_COBS_SIZE]{};

		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE; i++)
		{
			RxBuffer[i] = i;
		}

		const uint8_t outSize = CobsCodec::Encode(RxBuffer, cobsBuffer, LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);

		if (outSize != LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE + COBS_OVERHEAD)
		{
			return false;
		}

		memset(RxBuffer, 0, LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);

		const uint8_t inSize = CobsCodec::Decode(cobsBuffer, RxBuffer, outSize);

		if (inSize != LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
		{
			return false;
		}

		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE; i++)
		{
			if (RxBuffer[i] != i)
			{
				return false;
			}
		}

		return true;
	}
#endif
};

#endif