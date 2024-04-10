// Si446xRadioDriver.h

#ifndef _SI446X_RADIO_DRIVER_h
#define _SI446X_RADIO_DRIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "Si446xSpiDriver.h"

using namespace Si446x;


/// <summary>
/// Handles radio interrupts and dispatches radio events.
/// </summary>
/// <typeparam name="MaxRxSize"></typeparam>
/// <typeparam name="pinSDN"></typeparam>
/// <typeparam name="pinCS"></typeparam>
/// <typeparam name="pinCLK"></typeparam>
/// <typeparam name="pinMISO"></typeparam>
/// <typeparam name="pinMOSI"></typeparam>
/// <typeparam name="spiChannel"></typeparam>
/// <typeparam name="pinInterrupt"></typeparam>
template<const uint8_t MaxRxSize,
	const uint8_t pinCS,
	const uint8_t pinSDN,
	const uint8_t pinInterrupt,
	const uint8_t pinCLK = UINT8_MAX,
	const uint8_t pinMISO = UINT8_MAX,
	const uint8_t pinMOSI = UINT8_MAX,
	const uint8_t spiChannel = 0>
class Si446xRadioDriver : private Task
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
			if (State != ReadStateEnum::NoData)
			{
				Double = true;
			}
			else
			{
				Timestamp = timestamp;
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
	static constexpr uint32_t TX_CONSISTENCY_DELAY = 100;

private:
	Si446xSpiDriver<pinCS, pinSDN, pinCLK, pinMISO, pinMOSI, spiChannel> SpiDriver;

	void (*RadioInterrupt)(void) = nullptr;

	uint8_t InBuffer[MaxRxSize]{};

	AsyncReadInterruptsStruct AsyncReadInterrupts{};

	RadioEventsStruct RadioEvents{};

	RxFlowStruct RxFlow{};
	TxFlowStruct TxFlow{};
	HopFlowStruct HopFlow{};

protected:
	virtual void OnTxDone() {}

	virtual void OnRxReady(const uint8_t* data, const uint32_t timestamp, const uint8_t packetSize, const uint8_t rssiLatch) {}

public:
	Si446xRadioDriver(Scheduler& scheduler)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, SpiDriver()
	{}

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

	const bool SetTxPower(const uint8_t txPower)
	{
		return SpiDriver.SetTxPower(txPower);
	}

	const bool RadioStart(const uint8_t* configuration, const size_t configurationSize)
	{
		pinMode(pinSDN, INPUT_PULLUP);
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
		attachInterrupt(digitalPinToInterrupt(pinInterrupt), RadioInterrupt, FALLING);
		Task::enable();

		return SpiDriver.SetPacketSize(MaxRxSize, 1000);
	}

	void RadioStop()
	{
		SpiDriver.Stop();
		detachInterrupt(digitalPinToInterrupt(pinInterrupt));
		pinMode(pinInterrupt, INPUT);
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

		return SpiDriver.SpinWaitForResponse(timeoutMicros);
	}

	/// <summary>
	/// TxAvailable() must be called immediatelly before.
	/// </summary>
	const bool RadioTx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		// Last chance to detect any pending RX.
		if (RadioTaskBusy())
		{
			return false;
		}

		// Disable Rx temporarily.
		if (!SpiDriver.SetRadioState(RadioStateEnum::READY, 100))
		{
			return false;
		}

		if (!SpiDriver.SetPacketSize(packetSize, 250))
		{
			return false;
		}

		// Push data to radio FIFO and start transmission.

		
		if (!SpiDriver.RadioStartTx(data, packetSize, channel))
		{
			HopFlow.SetPending(micros());
			return false;
		}

		TxFlow.SetPending(micros());
		Task::enable();

		return true;
	}

public:
	virtual bool Callback() final
	{
		if (AsyncReadInterrupts.Pending())
		{
			ProcessAsyncRead();
		}
		else if (RadioEvents.Pending())
		{
			ProcessRadioEvents();
		}
		else
		{
			ProcessPendingFlows();
		}

		noInterrupts();
		if (RadioTaskBusy())
		{
			interrupts();
			Task::enableDelayed(0);

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
	void ProcessAsyncRead()
	{
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
				if (AsyncReadInterrupts.Double)
				{
#if defined(DEBUG_LOLA)
					Serial.println(F("AsyncReadInterrupts overflow"));
#endif
				}

				AsyncReadInterrupts.Clear();

				RadioEvents.StartTimestamp = AsyncReadInterrupts.Timestamp;
				if (RadioEvents.RxReady)
				{
					RadioEvents.RxTimestamp = RadioEvents.StartTimestamp;
#if defined(DEBUG_LOLA)
					if (RxFlow.Pending())
					{
						Serial.println(F("Rx overrun."));
					}
#endif
				}
			}
			break;
		default:
			break;
		}

		if (AsyncReadInterrupts.Pending()
			&& AsyncReadInterrupts.Elapsed(micros()) > 20000)
		{
			AsyncReadInterrupts.Clear();
			HopFlow.SetPending(micros());
#if defined(DEBUG_LOLA)
			Serial.println(F("Async Get Interrupt timed out."));
#endif
		}
	}

	void ProcessRadioEvents()
	{
		if (RadioEvents.TxDone)
		{
			RadioEvents.TxDone = false;
			if (TxFlow.Pending)
			{
				TxFlow.Clear();

				// Tipically runs on the first try.
				if (!SpiDriver.SetPacketSize(MaxRxSize, 100))
				{
#if defined(DEBUG_LOLA)
					Serial.println(F("Size restore failed."));
#endif
				}
				OnTxDone();
			}
			else
			{
#if defined(DEBUG_LOLA)
				Serial.println(F("Unexpected Tx Done"));
#endif
			}
		}

		if (RadioEvents.RxReady)
		{
			RadioEvents.RxReady = false;
			if (SpiDriver.GetRxFifoCount(RxFlow.Size)
				&& RxFlow.Size > 0 && RxFlow.Size <= MaxRxSize)
			{
				RxFlow.RssiLatch = SpiDriver.GetRssiLatchFast();
				RxFlow.SetPending(RadioEvents.RxTimestamp);
			}
			else
			{
				RxFlow.Clear();
				HopFlow.SetPending(micros());
				if (!SpiDriver.SetPacketSize(MaxRxSize, 1000))
				{
#if defined(DEBUG_LOLA)
					Serial.println(F("Size restore failed."));
#endif
				}
#if defined(DEBUG_LOLA)
				Serial.println(F("Rx invalid size."));
#endif
			}
		}

#if defined(DEBUG_LOLA)
		if (RadioEvents.RxStart)
		{
			RadioEvents.RxStart = false;
			Serial.println(F("Rx Start"));
		}

		if (RadioEvents.RxFail)
		{
			RadioEvents.RxFail = false;
			Serial.println(F("Rx Fail"));
		}

		if (RadioEvents.VccWarning)
		{
			RadioEvents.VccWarning = false;
			Serial.println(F("VccWarning"));
		}

		if (RadioEvents.CalibrationPending)
		{
			RadioEvents.CalibrationPending = false;
			Serial.println(F("CalibrationPending."));
		}

		if (RadioEvents.Error)
		{
			RadioEvents.Error = false;
			Serial.println(F("RF Error."));
		}
#else
		RadioEvents.RxStart = false;
		RadioEvents.RxFail = false;
		RadioEvents.VccWarning = false;
		RadioEvents.CalibrationPending = false;
		RadioEvents.Error = false;
#endif
	}

	void ProcessPendingFlows()
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
		else if (TxFlow.Pending
			&& ((micros() - TxFlow.Timestamp) > 20000))
		{
			//TODO: Handle failed Rx, needs restore?
#if defined(DEBUG_LOLA)
			Serial.println(F("Tx timeout."));
#endif
			TxFlow.Clear();
			if (!SpiDriver.SetPacketSize(MaxRxSize, 1000))
			{
#if defined(DEBUG_LOLA)
				Serial.println(F("Size restore failed."));
#endif
			}
			OnTxDone();
		}
		else if (HopFlow.Pending)
		{
			// Only set to Rx if Radio is not busy.
			if (!AsyncReadInterrupts.Pending())
			{
				uint32_t start = micros();
				//TODO: If Radio is already in RX but in a different channel, use fast hop instead.
				if (SpiDriver.RadioStartRx(HopFlow.Channel))
				{
					HopFlow.Clear();

					const uint32_t elapsed = micros() - start;
					Serial.print(F("Rx Hop took: "));
					Serial.print(elapsed);
					Serial.println(F(" us"));
					Serial.println();
					Serial.println();
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

	const bool RadioTaskBusy()
	{
		return TxFlow.Pending || HopFlow.Pending || RxFlow.Pending()
			|| RadioEvents.Pending() || AsyncReadInterrupts.Pending();
	}
};
#endif