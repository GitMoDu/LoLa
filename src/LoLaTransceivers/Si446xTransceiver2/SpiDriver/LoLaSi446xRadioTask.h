// LoLaSi446xRadioTask.h

#ifndef _LOLA_SI446X_RADIO_TASK_h
#define _LOLA_SI446X_RADIO_TASK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include "LoLaSi446xSupport.h"

#include "LoLaSi446xSpiRadio.h"


/// <summary>
/// Simplified Radio flow handler.
/// Timestamp source abstract.
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
	using RadioStateEnum = LoLaSi446x::RadioStateEnum;

private:
	void (*RadioInterrupt)(void) = nullptr;

	uint8_t InBuffer[MaxRxSize]{};

	LoLaSi446xSpiRadio<CsPin, SdnPin> SpiDriver;
	LoLaSi446x::RadioEventStruct RadioEvents{};

	LoLaSi446xSupport::InterruptEventStruct InterruptEvent{};

	LoLaSi446xSupport::RxEventStruct RxEvent{};
	LoLaSi446xSupport::TxEventStruct TxEvent{};

	LoLaSi446xSupport::RxHopStruct RxHop{};

	RadioStateEnum RadioState = RadioStateEnum::NO_CHANGE;

protected:
	virtual void OnTxDone() {}

	virtual void OnRxReady(const uint8_t* data, const uint32_t timestamp, const uint8_t packetSize, const uint8_t rssiLatch) {}

	virtual void OnWakeUpTimer() {}

public:
	LoLaSi446xRadioTask(Scheduler& scheduler, SPIClass* spiInstance)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, SpiDriver(spiInstance)
	{
		pinMode(InterruptPin, INPUT);
	}

	void RadioSetup(void (*onRadioInterrupt)(void))
	{
		RadioInterrupt = onRadioInterrupt;
	}

	void OnRadioInterrupt(const uint32_t timestamp)
	{
		Serial.println(F("OnRadioInterrupt"));
		if (InterruptEvent.Pending)
		{
			InterruptEvent.Double = true;
		}
		else
		{
			InterruptEvent.Timestamp = timestamp;
			InterruptEvent.Pending = true;
		}
		Task::enable();
	}

	const bool RadioStart()
	{
		//RadioStop();

		if (RadioInterrupt == nullptr
			|| !SpiDriver.Start())
		{
			Serial.println(F("LoLaSi446xRadioTask Failed."));
			return false;
		}

		RxEvent.Clear();
		TxEvent.Clear();
		RxHop.Clear();

		// Start listening to IRQ.
		pinMode(SdnPin, INPUT_PULLUP);
		interrupts();
		InterruptEvent.Clear();
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

	/// <summary>
	/// Checks internal state and pings radio over SPI to ensure Tx is possible.
	/// </summary>
	/// <returns></returns>
	const bool RadioTxAvailable()
	{
		if (InterruptEvent.Pending
			|| TxEvent.Pending
			|| RxEvent.Pending()
			)
		{
			return false;
		}

		const RadioStateEnum radioState = SpiDriver.GetRadioState();
		Serial.print(F("Radio State:"));
		Serial.println((uint8_t)radioState);

		return radioState == RadioStateEnum::RX;
		//return true;
	}

	void RadioRx(const uint8_t channel)
	{
		RxHop.SetPending(millis(), channel);
		Task::enable();
	}

	/// <summary>
	/// Assumes 
	/// Restores Radio state to Rx after Tx is complete?
	/// TxAvailable() must be called immediatelly before.
	/// </summary>
	const bool RadioTx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		// Disable Rx temporarily.
		if (!SpiDriver.SetRadioState(RadioStateEnum::READY))
		{
			//TODO: Hop pending.
			Serial.println(F("SetRadioState Failed."));
			return false;
		}

		// Clear the Tx FIFO.
		if (!SpiDriver.ClearTxFifo())
		{
			//TODO: Hop pending.
			// TODO: Restore state?
			Serial.println(F("ClearTxFifo Failed."));
			return false;
		}

		// Push data to radio FIFO.
		if (!SpiDriver.SetTxFifo(data, packetSize))
		{
			//TODO: Hop pending.
			// TODO: Restore state?
			Serial.println(F("SetTxFifo Failed."));
			return false;
		}

		// Set packet size for Tx.
		if (!SpiDriver.SetPacketSize(packetSize))
		{
			//TODO: Hop pending.
			// TODO: Restore state?
			Serial.println(F("SetPacketSize Failed."));
			return false;
		}

		// Start transmission.
		if (!SpiDriver.RadioStartTx(channel))
		{
			//TODO: Hop pending.
			// TODO: Restore state?
			Serial.println(F("RadioStartTx Failed."));
			return false;
		}

		// Reset Packet size for Rx.
		if (!SpiDriver.SetPacketSize(MaxRxSize))
		{
			//TODO: Hop pending.
			// TODO: Restore state?
			Serial.println(F("Restore SetPacketSize Failed."));
			return false;
		}

		TxEvent.Pending = true;
		TxEvent.Start = millis();
		Task::enable();

		return true;
	}

public:
	virtual bool Callback() final
	{
//		if (RxEvent.Pending())
//		{
//			if (RxEvent.Size > MaxRxSize)
//			{
//				// Invalid packet size.
//				RxEvent.Clear();
//				SpiDriver.ClearRxFifo();
//				//TODO: Restore state?
//			}
//			else
//			{
//				// Attempt to read and distribute any pending Rx Packet.
//				if (SpiDriver.GetRxFifo(InBuffer, RxEvent.Size))
//				{
//					OnRxReady(InBuffer, RxEvent.Timestamp, RxEvent.Size, RxEvent.RssiLatch);
//					SpiDriver.ClearRxFifo();
//					RxEvent.Clear();
//					//TODO: Restore state?
//				}
//				else if (millis() - RxEvent.Start > 5000)
//				{
//					// Timeout while reading Rx packet.
//					RxEvent.Clear();
//					SpiDriver.ClearRxFifo();
//					//TODO: Restore state?
//				}
//			}
//		}
//
//		// Check and clear Tx on timeout.
//		if (TxEvent.Pending
//			&& (millis() - TxEvent.Start > 10))
//		{
//			TxEvent.Clear();
//			OnTxDone();
//		}
//
//		// Process any pending interrupt.
//		if (InterruptEvent.Pending)
//		{
//			if (InterruptEvent.Double)
//			{
//#if defined(DEBUG_LOLA)
//				Serial.println(F("Si446x Double Interrupt Event"));
//#endif
//			}
//
//			// Ask Radio what event triggered the interrupt.
//			if (false && SpiDriver.GetRadioEvents(RadioEvents))
//			{
//				// Just woke up, just notify.
//				if (RadioEvents.WakeUp)
//				{
//					OnWakeUpTimer();
//				}
//
//				if (RadioEvents.RxStart)
//				{
//					if (RxEvent.Pending())
//					{
//						//TODO: Handle overruns of previous Rx?.
//					}
//					else
//					{
//						RxEvent.Timestamp = InterruptEvent.Timestamp;
//						RxEvent.RssiLatch = SpiDriver.GetLatchedRssi();
//					}
//				}
//
//				if (RadioEvents.RxReady)
//				{
//					if (RxEvent.Pending())
//					{
//						//TODO: Handle overruns of previous Rx?.
//					}
//					else
//					{
//						// Get packet size.
//						const uint8_t packetSize = SpiDriver.GetRxLatchedSize();
//						if (packetSize > 0)
//						{
//							RxEvent.SetPending(millis(), packetSize);
//						}
//						else
//						{
//							//TODO: Restore state?
//						}
//					}
//				}
//
//				if (RadioEvents.RxFail)
//				{
//					SpiDriver.ClearRxFifo();
//					//TODO: Restore state?
//				}
//
//				if (RadioEvents.TxDone)
//				{
//					TxEvent.Clear();
//					if (TxEvent.Pending)
//					{
//						OnTxDone();
//					}
//					else
//					{
//						//TODO: Restore internal state?
//					}
//				}
//
//				InterruptEvent.Clear();
//			}
//		}

		if (RxHop.Pending() && !TxEvent.Pending)
		{
			if ((millis() - RxHop.Start > 10))
			{
				RxHop.Clear();
#if defined(DEBUG_LOLA)
				Serial.println(F("Si446x Rx Hop timed out."));
#endif
			}
			else
			{
				RadioStateEnum radioState = SpiDriver.GetRadioState();
				Serial.print(F("Pre-Hop Radio State:"));
				Serial.println((uint8_t)radioState);

				//TODO: Check if any even pending and defer Rx.
				//TODO: Check if already on Rx State.
				// If so, use Hop command, as it is much faster.
				//if (CurrentChannel != rawChannel) {
				//	CurrentChannel = rawChannel;
				//	HopPending = true;
				//	Task::enable();
				//}

				if (SpiDriver.SetRadioState(RadioStateEnum::SPI_ACTIVE)
					//&& SpiDriver.ClearRxFifo()  //TODO: Needed?
					&& SpiDriver.RadioStartRx(RxHop.Channel)) // Disable Tx temporarily.
				{
#if defined(DEBUG_LOLA)
					Serial.println(F("Si446x Rx Hop Ok."));
					radioState = SpiDriver.GetRadioState();
					Serial.print(F("Post-Hop Radio State:"));
					Serial.println((uint8_t)radioState);
#endif
					RxHop.Clear();
				}
				else
				{
					//TODO: Restore state?
				}
			}
		}

		// Reset packet size for Rx.
		////TODO: Needed? And in Tx?
		//if (!SpiDriver.SetPacketSize(MaxRxSize))
		//{
		//	//TODO: Hop pending.
		//	return false;
		//}


		if (InterruptEvent.Pending
			|| TxEvent.Pending
			|| RxEvent.Pending()
			|| RxHop.Pending())
		{
			Task::enable();
			return true;
		}
		else
		{
			Task::disable();
			return false;
		}
	}
};
#endif