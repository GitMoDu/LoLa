// LoLaSi446xRadioTask.h

#ifndef _LOLA_SI446X_RADIO_TASK_h
#define _LOLA_SI446X_RADIO_TASK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "LoLaSi446xSpiDriver.h"



using namespace LoLaSi446x;

/// <summary>
/// Handles and dispatches radio interrupts.
/// </summary>
/// <typeparam name="MaxRxSize"></typeparam>
/// <typeparam name="CsPin"></typeparam>
/// <typeparam name="SdnPin"></typeparam>
/// <typeparam name="InterruptPin"></typeparam>
template<const uint8_t MaxRxSize,
	const uint8_t CsPin,
	const uint8_t SdnPin,
	const uint8_t InterruptPin>
class LoLaSi446xRadioTask : private Task
{
private:
	enum class ReadStateEnum
	{
		NoData,
		TryPrepare,
		TryRead
	};

	struct AsyncReadInterruptsStruct
	{
		volatile uint32_t Timestamp = 0;
		volatile ReadStateEnum State = ReadStateEnum::NoData;
		volatile bool Double = false;

		const bool Pending()
		{
			return State != ReadStateEnum::NoData;
		}

		void SetPending(const uint32_t timestamp)
		{
			Timestamp = timestamp;
			if (State != ReadStateEnum::NoData)
			{
				Double = true;
			}
			else
			{
				State = ReadStateEnum::TryPrepare;
			}
		}

		void Clear()
		{
			State = ReadStateEnum::NoData;
			Double = false;
		}

		const uint32_t Elapsed(const uint32_t timestamp)
		{
			return timestamp - Timestamp;
		}
	};

	struct HopFlowStruct
	{
	public:
		uint32_t Timestamp = 0;
		uint8_t Channel = 0;
		bool Pending = false;

	public:
		void Clear()
		{
			Pending = false;
		}

		void SetPending(const uint32_t startTimestamp)
		{
			Timestamp = startTimestamp;
			Pending = true;
		}

		void SetPending(const uint32_t startTimestamp, const uint8_t channel)
		{
			SetPending(startTimestamp);
			Channel = channel;
		}
	};

	struct TxFlowStruct
	{
		uint32_t Timestamp = 0;
		bool Pending = false;

		void SetPending(const uint32_t timestamp)
		{
			Timestamp = timestamp;
			Pending = true;
		}

		void Clear()
		{
			Pending = false;
		}
	};

	struct RxFlowStruct
	{
		uint32_t Timestamp = 0;
		uint8_t Size = 0;
		uint8_t RssiLatch = 0;

		void SetPending(const uint32_t startTimestamp)
		{
			Timestamp = startTimestamp;
		}

		const bool Pending()
		{
			return Size > 0;
		}

		void Clear()
		{
			Size = 0;
		}
	};

private:
	LoLaSi446xSpiDriver<CsPin, SdnPin> SpiDriver;

	void (*RadioInterrupt)(void) = nullptr;

	uint8_t InBuffer[MaxRxSize]{};

	AsyncReadInterruptsStruct AsyncReadInterrupts{};

	LoLaSi446x::RadioEventsStruct RadioEvents{};

	RxFlowStruct RxFlow{};
	TxFlowStruct TxFlow{};
	HopFlowStruct HopFlow{};

protected:
	virtual void OnTxDone() {}

	virtual void OnRxReady(const uint8_t* data, const uint32_t timestamp, const uint8_t packetSize, const uint8_t rssiLatch) {}

public:
	LoLaSi446xRadioTask(Scheduler& scheduler, SPIClass* spiInstance)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, SpiDriver(spiInstance)
	{
		pinMode(InterruptPin, INPUT);
	}

	void OnRadioInterrupt(const uint32_t timestamp)
	{
		AsyncReadInterrupts.SetPending(timestamp);
		Task::enable();
	}

protected:
	void RadioSetup(void (*onRadioInterrupt)(void))
	{
		RadioInterrupt = onRadioInterrupt;
	}

	const bool RadioStart(const uint8_t* configuration, const size_t configurationSize)
	{
		pinMode(SdnPin, INPUT_PULLUP);

		RadioStop();

		if (RadioInterrupt == nullptr
			|| !SpiDriver.Start(configuration, configurationSize))
		{
			return false;
		}

		AsyncReadInterrupts.Clear();
		RadioEvents.Clear();
		RxFlow.Clear();
		TxFlow.Clear();
		HopFlow.Clear();

		// Start listening to IRQ.
		interrupts();
		attachInterrupt(digitalPinToInterrupt(InterruptPin), RadioInterrupt, FALLING);
		Task::enable();

		return true;
	}

	void RadioStop()
	{
		SpiDriver.Stop();
		detachInterrupt(digitalPinToInterrupt(InterruptPin));
		pinMode(InterruptPin, INPUT);
		interrupts();
		Task::disable();
	}

	void RadioRx(const uint8_t channel)
	{
		if (HopFlow.Pending)
		{
			if (HopFlow.Channel != channel)
			{
				HopFlow.Channel = channel;
			}
		}
		else
		{
			HopFlow.SetPending(micros(), channel);
		}
		Task::enableDelayed(0);
	}

	/// <summary>
	/// Checks internal state and pings radio over SPI to ensure Tx is possible.
	/// </summary>
	/// <returns></returns>
	const bool RadioTxAvailable(const uint32_t timeoutMicros = 50)
	{
		if (RadioTaskBusy())
		{
			return false;
		}

		switch (SpiDriver.GetRadioStateFast())
		{
		case RadioStateEnum::RX:
		case RadioStateEnum::SPI_ACTIVE:
		case RadioStateEnum::READY:
			return SpiDriver.SpinWaitClearToSend(timeoutMicros);
			break;
		default:
			break;
		}

		return false;
	}

	/// <summary>
	/// Assumes 
	/// Restores Radio state to Rx after Tx is complete?
	/// TxAvailable() must be called immediatelly before.
	/// </summary>
	const bool RadioTx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		if (RadioTaskBusy())
		{
			return false;
		}

		TxFlow.SetPending(micros());

		// Disable Rx temporarily.
		if (!SpiDriver.SetRadioState(RadioStateEnum::READY, 50))
		{
			Serial.println(F("SetRadioState Failed."));
			return false;
		}

		// Clear the Tx FIFO.
		// TODO: remove if not needed.
		if (!SpiDriver.ClearTxFifo(25))
		{
			Serial.println(F("Clear TxFifo Failed."));
			return false;
		}

		if (!SpiDriver.GetRadioEvents(RadioEvents, 50))
		{
			Serial.println(F("Clear RadioEvents Failed."));
			return false;
		}
		RadioEvents.Clear();


		// Push data to radio FIFO.
		//if (!SpiDriver.SetTxFifo(data, packetSize))
		//{
		//	//TODO: Hop pending.
		//	// TODO: Restore state?
		//	Serial.println(F("SetTxFifo Failed."));
		//	return false;
		//}

		// Start transmission.
		if (!SpiDriver.RadioStartTx(data, packetSize, channel, 200))
		{
			//TODO: Hop pending.
			// TODO: Restore state?
			Serial.println(F("RadioStartTx Failed."));
			return false;
		}

		Task::enable();

		return true;
	}

public:
	virtual bool Callback() final
	{
		if (AsyncReadInterrupts.Pending())
		{
			if (AsyncReadInterrupts.Double)
			{
#if defined(DEBUG_LOLA)
				Serial.println(F("AsyncReadInterrupts overflow"));
#endif
				AsyncReadInterrupts.Double = false;
				AsyncReadInterrupts.State = ReadStateEnum::TryPrepare;
			}

			// Event read takes too long to do synchronously.
			switch (AsyncReadInterrupts.State)
			{
			case ReadStateEnum::TryPrepare:
				if (SpiDriver.TryPrepareGetRadioEvents())
				{
					AsyncReadInterrupts.State = ReadStateEnum::TryRead;

					if (RadioEvents.Pending())
					{
#if defined(DEBUG_LOLA)
						Serial.println(F("Radio Events overflow"));
#endif
					}
				}
				break;
			case ReadStateEnum::TryRead:
				if (SpiDriver.TryGetRadioEvents(RadioEvents))
				{
					AsyncReadInterrupts.Clear();

					if (RadioEvents.RxReady)
					{
						RadioEvents.RxTimestamp = AsyncReadInterrupts.Timestamp;
#if defined(DEBUG_LOLA)
						if (RxFlow.Pending())
						{
							Serial.println(F("Rx overrun."));
						}
#endif
					}
					RadioEvents.StartTimestamp = AsyncReadInterrupts.Timestamp;
				}
				break;
			default:
				break;
			}

			if (AsyncReadInterrupts.Pending()
				&& AsyncReadInterrupts.Elapsed(micros()) > 5000)
			{
				AsyncReadInterrupts.Clear();
				HopFlow.SetPending(micros());
#if defined(DEBUG_LOLA)
				Serial.println(F("Async Get Interrupt timed out."));
#endif
			}
		}

		if (RadioEvents.Pending())
		{
			if (RadioEvents.RxStart)
			{
				RadioEvents.RxStart = false;
				Serial.print(micros());
				Serial.println(F("\tEvent: Rx Start"));
			}

			if (RadioEvents.RxFail)
			{
				RadioEvents.RxFail = false;
				Serial.print(micros());
				Serial.println(F("\tEvent: Rx Fail"));
			}

			if (RadioEvents.VccWarning)
			{
				RadioEvents.VccWarning = false;
				Serial.println(F("VccWarning Event."));
			}

			if (RadioEvents.TxDone)
			{
				RadioEvents.TxDone = false;
				Serial.print(micros());
				Serial.println(F("\tEvent: Tx Done"));
				if (TxFlow.Pending)
				{
					TxFlow.Clear();
					//TODO: Set radio state to TX ready.
					if (!SpiDriver.SetPacketSize(MaxRxSize, 200))
					{
#if defined(DEBUG_LOLA)
						Serial.println(F("Restore SetPacketSize Failed."));
#endif
					}
					OnTxDone();
				}
				else
				{
#if defined(DEBUG_LOLA)
					Serial.println(F("Si446x Bad Tx Done"));
#endif
				}
			}

			if (RadioEvents.RxReady)
			{
				const uint32_t rxFlowStart = micros();
				RadioEvents.RxReady = false;
				//Serial.print(micros());
				//Serial.println(F("\tEvent: Rx Ready"));

				if (SpiDriver.GetRxFifoCount(RxFlow.Size)
					&& RxFlow.Size > 0 && RxFlow.Size <= MaxRxSize)
				{
					if (!SpiDriver.GetRssiLatchFast(RxFlow.RssiLatch, 25))
					{
#if defined(DEBUG_LOLA)
						Serial.println(F("Rx with no rssi."));
#endif
					}

					const uint32_t rxFlowDuration = micros() - rxFlowStart;
					RxFlow.SetPending(RadioEvents.RxTimestamp);

					Serial.print(F("RxFlow Prepare took "));
					Serial.print(rxFlowDuration);
					Serial.println(F(" us."));
				}
				else
				{
					RxFlow.Clear();
					HopFlow.SetPending(micros(), HopFlow.Channel);
					Serial.println(F("Rx invalid size."));
				}
			}

			if (RadioEvents.Pending()
				&& (micros() - RadioEvents.StartTimestamp) > 10000)
			{
				RadioEvents.Clear();
				HopFlow.SetPending(micros());
#if defined(DEBUG_LOLA)
				Serial.println(F("RadioEvents processing timed out."));
#endif
			}
		}
		else
		{
			if (RxFlow.Pending())
			{
				if (SpiDriver.GetRxFifo(InBuffer, RxFlow.Size))
				{
					OnRxReady(InBuffer, RxFlow.Timestamp, RxFlow.Size, RxFlow.RssiLatch);
				}
				else
				{
#if defined(DEBUG_LOLA)
					Serial.println(F("Get Rx failed."));
#endif
				}
				RxFlow.Clear();
			}

			// Check for timeout on pending Tx.
			if (TxFlow.Pending
				&& ((micros() - TxFlow.Timestamp) > 10000))
			{
				//TODO: Handle failed Rx, needs restore?
#if defined(DEBUG_LOLA)
				Serial.println(F("Tx timeout."));
#endif
				TxFlow.Clear();
				if (!SpiDriver.SetPacketSize(MaxRxSize, 200))
				{
#if defined(DEBUG_LOLA)
					Serial.println(F("Restore PacketSize Failed."));
#endif
				}
				OnTxDone();
			}

			if (HopFlow.Pending)
			{
				if (!TxFlow.Pending
					&& !RxFlow.Pending()
					&& !AsyncReadInterrupts.Pending())
				{
					uint32_t start = micros();
					//TODO: If Radio is already in RX and in the same channel, skip.
					//TODO: If Radio is already in RX but in a differebt channel, use fast hop instead.
					if (SpiDriver.RadioStartRx(HopFlow.Channel))
					{
						HopFlow.Clear();

						const uint32_t elapsed = micros() - start;
						Serial.print(F("Rx Hop took: "));
						Serial.print(elapsed);
						Serial.println(F(" us"));
					}
					else
					{
#if defined(DEBUG_LOLA)
						Serial.println(F("Rx Hop Failed."));
#endif
					}
				}
				else if (micros() - HopFlow.Timestamp > 10000)
				{
					HopFlow.Clear();
#if defined(DEBUG_LOLA)
					Serial.println(F("Rx Hop Timed out."));
#endif
				}
			}
		}

		noInterrupts();
		if (RadioTaskBusy())
		{
			Task::enableDelayed(0);
			interrupts();
			return true;
		}
		else
		{
			Task::disable();
			interrupts();

			return false;
		}
	}

private:
	const bool RadioTaskBusy()
	{
		return TxFlow.Pending || HopFlow.Pending || RxFlow.Pending()
			|| RadioEvents.Pending() || AsyncReadInterrupts.Pending();
	}
};
#endif