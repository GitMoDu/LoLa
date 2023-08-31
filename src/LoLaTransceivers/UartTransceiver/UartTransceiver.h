// UartTransceiver.h

#ifndef _LOLA_UART_TRANSCEIVER_h
#define _LOLA_UART_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>

#include "cobs\cobs.h"

/// <summary>
/// UART/Serial LoLa Transceiver.
/// Packet start detection and timestamping relies on interrupt pin.
/// Packets are framed using Consistent Overhead Byte Stuffing (COBS).
/// The encoded data consists only of bytes with values from 0x01 to 0xFF.
/// The 0x00 byte is be used to indicate packet end.
/// Constant overhead.
/// Detects malformed input data.
/// https://github.com/charlesnicholson/nanocobs
/// </summary>
/// <typeparam name="SerialType">Type definition of HardwareSerial.</typeparam>
/// <typeparam name="BaudRate">At least MIN_BAUD_RATE.</typeparam>
/// <typeparam name="RxInterruptPin">The pin tied to UART Rx, for packet start detection and timestamping.</typeparam>
/// <typeparam name="RxBufferSize">UART hardware buffer size.</typeparam>
/// <typeparam name="TxBufferSize">UART hardware buffer size.</typeparam>
template<typename SerialType,
	const uint32_t BaudRate,
	const uint8_t RxInterruptPin,
	const size_t RxBufferSize = 64,
	const size_t TxBufferSize = 64>
class UartTransceiver final
	: private Task, public virtual ILoLaTransceiver
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
	static constexpr uint8_t EoF = 0;
	static constexpr uint8_t MAX_COBS_SIZE = COBS_ENCODE_MAX(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	static constexpr uint8_t COBS_OVERHEAD = MAX_COBS_SIZE - LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE;
	static constexpr uint8_t COBS_IN_PLACE_OFFSET = 1;

private:
	static constexpr uint32_t REFERENCE_LONG = 35308;
	static constexpr uint32_t REFERENCE_SHORT = 8230;
	static constexpr uint16_t REFERENCE_BAUD_RATE = 9600;
	static constexpr uint16_t MIN_BAUD_RATE = 80000;

private:
	static constexpr uint16_t DurationShort = (uint32_t)((REFERENCE_SHORT * REFERENCE_BAUD_RATE) / BaudRate);
	static constexpr uint16_t DurationLong = (uint32_t)((REFERENCE_LONG * REFERENCE_BAUD_RATE) / BaudRate);
	static constexpr uint16_t DurationRange = DurationLong - DurationShort;

	static constexpr uint16_t ByteDuration = (DurationLong / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	static constexpr uint16_t BitDuration = (DurationLong / ((uint16_t)LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE * 8));

private:
	static constexpr uint16_t RxWaitDuration = 1500;
	static constexpr uint16_t TxWaitDuration = 100;

	static constexpr uint16_t LineDuration = BitDuration;

	static constexpr uint8_t CalibrationSamples = 50;

	uint16_t EncodeBaseDuration = 1;
	uint16_t EncodeRangeDuration = 1;

private:
	SerialType* IO;

	ILoLaTransceiverListener* Listener = nullptr;

	uint8_t RxBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};
	uint8_t TxBuffer[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};

	volatile uint32_t RxStartTimestamp = 0;
	volatile uint32_t TxStartTimestamp = 0;

	uint32_t TxEndTimestamp = 0;

	volatile RxStateEnum RxState = RxStateEnum::NoRx;
	TxStateEnum TxState = TxStateEnum::NoTx;

	uint8_t ByteBuffer = 0;
	uint8_t RxSize = 0;
	uint8_t TxSize = 0;

private:
	void (*OnRxInterrupt)(void) = nullptr;
	volatile bool DriverEnabled = false;

public:
	UartTransceiver(Scheduler& scheduler, SerialType* io)
		: ILoLaTransceiver()
		, Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, IO(io)
	{
#if defined(RX_TEST_PIN)
		digitalWrite(RX_TEST_PIN, LOW);
		pinMode(RX_TEST_PIN, OUTPUT);
#endif

#if defined(TX_TEST_PIN)
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

		if (DriverEnabled && RxState == RxStateEnum::NoRx)
		{
#if defined(RX_TEST_PIN)
			digitalWrite(RX_TEST_PIN, HIGH);
#endif
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
			switch (RxState)
			{
			case RxStateEnum::NoRx:
				break;
			case RxStateEnum::RxStart:
				// Wait for at least the base transmit and 1 byte period.
				if (IO->available())
				{
					RxSize = 0;
					RxState = RxStateEnum::RxEnd;
				}
				else if (micros() - RxStartTimestamp > ((uint32_t)LineDuration + ByteDuration + RxWaitDuration))
				{
					// Too much time elapsed before at least one byte is available, discard.
					OnRxDone(false);
				}
				break;
			case RxStateEnum::RxEnd:
				while (RxState == RxStateEnum::RxEnd
					&& IO->available())
				{
					ByteBuffer = IO->read();
					RxBuffer[RxSize++] = ByteBuffer;

					if (ByteBuffer == EoF)
					{
						RxSize = CobsDecodeInPlace();

						if (RxSize < LoLaPacketDefinition::MIN_PACKET_SIZE || RxSize > LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
						{
							// Invalid size packet, discard.
							OnRxDone(false);
						}
						else
						{
							OnRxDone(true);
						}
					}
					else
					{
						if (RxSize > MAX_COBS_SIZE)
						{
							// Invalid COBS size, discard.
							OnRxDone(false);
						}
					}
				}

				if (RxState == RxStateEnum::RxEnd
					&& micros() - RxStartTimestamp > ((uint32_t)LineDuration + DurationLong + RxWaitDuration))
				{
					// Too much time elapsed before packet EoF found, discard.
					OnRxDone(false);
				}
				break;
			default:
				break;
			}

			switch (TxState)
			{
			case TxStateEnum::NoTx:
				break;
			case TxStateEnum::TxStart:
				// Wait for Tx buffer to be free after the minimum duration.
				if ((micros() - TxStartTimestamp > LineDuration + ByteDuration)
					&& (IO->availableForWrite() >= (TxBufferSize - 1)))
				{
					IO->flush();
					TxState = TxStateEnum::TxEnd;
#if defined(TX_TEST_PIN)
					digitalWrite(TX_TEST_PIN, LOW);
#endif
				}
				break;
			case TxStateEnum::TxEnd:
				TxEndTimestamp = micros();
				Listener->OnTx();
				TxState = TxStateEnum::NoTx;
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

		return Listener != nullptr && Calibrate();
	}

	virtual const bool Start() final
	{
		if (IO != nullptr
			&& Listener != nullptr
			&& OnRxInterrupt != nullptr
			&& BaudRate >= MIN_BAUD_RATE
			)
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
	/// UART isn't limited by Rx.
	/// Tx limited by pause after last Tx ended.
	/// </summary>
	/// <returns></returns>
	virtual const bool TxAvailable() final
	{
		return DriverEnabled
			&& TxState == TxStateEnum::NoTx
			&& micros() - TxEndTimestamp > TxWaitDuration;
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
		if (TxAvailable())
		{
			TxSize = CobsEncode(data, packetSize);

			if (TxSize > 0)
			{
#if defined(TX_TEST_PIN)
				digitalWrite(TX_TEST_PIN, HIGH);
#endif
				TxStartTimestamp = micros();
				if (IO->write(TxBuffer, TxSize) == TxSize)
				{
					TxState = TxStateEnum::TxStart;
					Task::enable();

					return true;
				}
				else
				{
					IO->flush();
					TxState = TxStateEnum::NoTx;
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
#if defined(RX_TEST_PIN)
		digitalWrite(RX_TEST_PIN, LOW);
#endif
		if (rxGood)
		{
			Listener->OnRx(&RxBuffer[COBS_IN_PLACE_OFFSET], RxStartTimestamp, RxSize, UINT8_MAX);
		}
		else
		{
			Listener->OnRxLost(RxStartTimestamp);
		}

		// Clear input buffer.
		while (IO->available())
		{
			IO->read();
		}

		RxState = RxStateEnum::NoRx;
		EnableInterrupt();
		Task::enable();
	}

	const uint8_t CobsDecodeInPlace()
	{
		return CobsDecodeInPlace(RxSize);
	}

	const uint8_t CobsDecodeInPlace(const uint_fast8_t size)
	{
		unsigned decodedSize = size;
		if (cobs_decode_inplace(
			(void*)RxBuffer,
			decodedSize) == COBS_RET_SUCCESS)
		{
			return decodedSize - COBS_OVERHEAD;
		}
		else
		{
			return 0;
		}
	}

	const uint8_t CobsDecode(const uint8_t* input, const uint_fast8_t size)
	{
		unsigned decodedSize = 0;
		if (cobs_decode(
			(void const*)input,
			(unsigned)size,
			(void*)RxBuffer,
			(unsigned)LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE,
			&decodedSize) == COBS_RET_SUCCESS)
		{
			return decodedSize;
		}
		else
		{
			return 0;
		}
	}

	const uint8_t CobsEncode(const uint8_t* input, const uint_fast8_t size)
	{
		unsigned encodedSize = 0;
		if (cobs_encode(
			(void const*)input,
			(unsigned)size,
			(void*)TxBuffer,
			(unsigned)MAX_COBS_SIZE,
			&encodedSize) == COBS_RET_SUCCESS)
		{
			return encodedSize;
		}
		else
		{
			return 0;
		}
	}

	const bool Calibrate()
	{
		uint32_t start, end, duration;
		uint8_t benchIndex = 0;

		const uint32_t calibrationStart = micros();
		start = micros();
		for (uint_fast8_t i = 0; i < CalibrationSamples; i++)
		{
			benchIndex = CobsEncode(RxBuffer, LoLaPacketDefinition::MIN_PACKET_SIZE);
			RxBuffer[0] = benchIndex;
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
			benchIndex = CobsEncode(RxBuffer, LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
			RxBuffer[0] = benchIndex;
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

		const uint32_t calibrationEnd = micros();

#if defined(DEBUG_LOLA)
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
};
#endif