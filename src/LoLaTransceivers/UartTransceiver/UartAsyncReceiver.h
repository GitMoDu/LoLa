// UartAsyncReceiver.h

#ifndef _LOLA_UART_TRANSCEIVER_ASYNC_RECEIVER__h
#define _LOLA_UART_TRANSCEIVER_ASYNC_RECEIVER__h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

struct IUartListener
{
	virtual void OnUartRx(const uint32_t timestamp, const uint8_t* data, const uint8_t size) {}
};

template<typename SerialType,
	const uint8_t RxInterruptPin,
	const uint8_t RxBufferSize,
	const uint32_t RxTimeoutMicros,
	const uint32_t RxPauseMicros,
	const uint32_t RxWaitMicros,
	const uint32_t MaxHoldMicros
>
class UartAsyncReceiver final : private TS::Task
{
private:
	static constexpr uint8_t Delimiter = 0;

	enum class StateEnum : uint8_t
	{
		Disabled,
		WaitingForInterrupt,
		RxStart,
		RxParse,
		RxEnd,
		Clear,
		ClearDelay
	};

private:
	SerialType& IO;

	IUartListener& Listener;

private:
	void (*RxInterrupt)(void) = nullptr;

private:
	uint8_t Buffer[RxBufferSize]{};

	uint32_t ClearStart = 0;
	volatile uint32_t RxTimestamp = 0;

	volatile StateEnum State = StateEnum::Disabled;

	uint_fast8_t RxSize = 0;

public:
	UartAsyncReceiver(TS::Scheduler& scheduler, SerialType& io, IUartListener& listener)
		: TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, IO(io)
		, Listener(listener)
	{
	}

	void SetupInterrupt(void (*rxInterrupt)(void))
	{
		pinMode(RxInterruptPin, INPUT);

		RxInterrupt = rxInterrupt;
	}

	const bool Start()
	{
		if (Buffer != nullptr
			&& RxInterrupt != nullptr)
		{
			State = StateEnum::Clear;
			TS::Task::enable();

			return true;
		}
		else
		{
			return false;
		}
	}

	void Stop()
	{
		TS::Task::disable();
	}

	void OnRxInterrupt()
	{
		switch (State)
		{
		case StateEnum::WaitingForInterrupt:
			RxTimestamp = micros();
#if defined(RX_TEST_PIN)
			digitalWrite(RX_TEST_PIN, HIGH);
#endif
			DisableInterrupt();
			State = StateEnum::RxStart;
			TS::Task::enable();
			break;
		case StateEnum::RxParse:
		case StateEnum::RxStart:
		case StateEnum::RxEnd:
		case StateEnum::Clear:
		case StateEnum::ClearDelay:
		case StateEnum::Disabled:
		default:
			break;
		}
	}

	bool Callback() final
	{
		switch (State)
		{
		case StateEnum::RxStart:
			if (IO.available())
			{
				// UART packets must start and end with a delimiter.
				if (IO.read() == Delimiter)
				{
					RxSize = 0;
					State = StateEnum::RxParse;
				}
				else
				{
					//Serial.println(F("Rx start not a delimiter"));
					State = StateEnum::Clear;
				}
			}
			else if ((micros() - RxTimestamp) > RxWaitMicros)
			{
				//Serial.println(F("Rx start failed"));
				State = StateEnum::Clear;
			}
			break;
		case StateEnum::RxParse:
			// Parse input buffer, in passes if necessary.
			ParseRx();
			break;
		case StateEnum::RxEnd:
			Listener.OnUartRx(RxTimestamp, Buffer, RxSize);
			State = StateEnum::Clear;
			break;
		case StateEnum::Clear:
			ClearStart = micros();
			if (IO.available())
			{
				// Clear input buffer, in passes if necessary.
				while (((micros() - ClearStart) < MaxHoldMicros)
					&& IO.available())
				{
					IO.read();
				}
			}
			else
			{
				State = StateEnum::ClearDelay;
			}
			break;
		case StateEnum::ClearDelay:
			if (IO.available())
			{
				State = StateEnum::Clear;
			}
			else if (micros() - ClearStart > RxPauseMicros)
			{
				State = StateEnum::WaitingForInterrupt;
#ifdef RX_TEST_PIN
				digitalWrite(RX_TEST_PIN, LOW);
#endif
				EnableInterrupt();
			}
			break;
		case StateEnum::WaitingForInterrupt:
		case StateEnum::Disabled:
		default:
			TS::Task::disable();
			break;
		}

		return true;
	}

private:
	void ParseRx()
	{
		if ((micros() - RxTimestamp) > RxTimeoutMicros)
		{
			// Too much time elapsed before packet EoF found, discard.
			//Serial.print(F("Rx timeout."));
			State = StateEnum::Clear;
		}
		else
		{
			uint32_t start = micros();

			while (IO.available()
				&& ((micros() - start) < MaxHoldMicros))
			{
				const uint8_t in = IO.read();

				// UART packets must start and end with a delimiter.
				if (in == Delimiter)
				{
					if (RxSize > 0)
					{
						State = StateEnum::RxEnd;
					}
					else
					{
						//Serial.println(F("Rx underflow"));
						State = StateEnum::Clear;
					}
				}
				else
				{
					if (RxSize < RxBufferSize)
					{
						Buffer[RxSize++] = in;
					}
					else
					{
						//Serial.println(F("Rx overflow"));
						State = StateEnum::Clear;
					}
				}
			}
		}
	}

	void EnableInterrupt()
	{
		attachInterrupt(digitalPinToInterrupt(RxInterruptPin), RxInterrupt, CHANGE);
		interrupts();
	}

	void DisableInterrupt()
	{
		detachInterrupt(digitalPinToInterrupt(RxInterruptPin));
	}
};

#endif