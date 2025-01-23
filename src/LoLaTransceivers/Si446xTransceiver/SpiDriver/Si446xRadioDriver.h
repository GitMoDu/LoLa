// Si446xRadioDriver.h

#ifndef _SI446X_RADIO_DRIVER_h
#define _SI446X_RADIO_DRIVER_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include "Si446xSpiDriver.h"

using namespace Si446x;

/// <summary>
/// Hosts and handles SPI radio driver.
/// Handles interrupts and dispatches radio events.
/// </summary>
/// <typeparam name="MaxRxSize"></typeparam>
/// <typeparam name="pinSDN"></typeparam>
/// <typeparam name="pinCS"></typeparam>
template<const uint8_t MaxRxSize,
	const uint8_t pinCS,
	const uint8_t pinSDN,
	const uint8_t pinInterrupt>
class Si446xRadioDriver : protected TS::Task
{
private:
	static constexpr uint8_t PH_FLAG = (uint8_t)INT_CTL_PH::PACKET_SENT_EN | (uint8_t)INT_CTL_PH::PACKET_RX_EN;
	static constexpr uint8_t MODEM_FLAG = 0;
	static constexpr uint8_t CHIP_FLAG = 0;

	/// <summary>
	/// Fast Read Register specification for the driver.
	/// </summary>
	enum class FrrEnum : uint8_t
	{
		LatchedRssi = (uint8_t)Command::FRR_A_READ,
		CurrentState = (uint8_t)Command::FRR_B_READ,
		PacketHandlerInterrupts = (uint8_t)Command::FRR_C_READ,
		ModemInterrupts = (uint8_t)Command::FRR_D_READ,
	};

	enum class StateEnum
	{
		Disabled,
		WaitingForRxInterrupt,
		PendingRxInterrupt,
		RxDelivery,
		TxReady,
		WaitingForTxInterrupt,
		PendingTxInterrupt,
		Hop,
		HopStartRx,
	};

private:
	Si446xSpiDriver<pinCS, pinSDN> SpiDriver;

private:
	void (*RadioInterrupt)(void) = nullptr;

private:
	uint8_t InBuffer[MaxRxSize]{};

	volatile uint32_t RxTimestamp = 0;
	uint32_t TxTimestamp = 0;
	uint16_t InterruptErrors = 0;

	volatile StateEnum State = StateEnum::Disabled;

	uint8_t RxRssi = 0;
	uint8_t RxSize = 0;
	uint8_t HopChannel = 0;

protected:
	virtual void OnTxDone() {}

	virtual void OnRxReady(const uint8_t* data, const uint32_t timestamp, const uint8_t packetSize, const uint8_t rssiLatch) {}

	virtual void OnRadioError(const RadioErrorCodeEnum radioFailCode) {}

public:
	Si446xRadioDriver(TS::Scheduler& scheduler, SPIClass& spiInstance)
		: TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, SpiDriver(spiInstance, PH_FLAG, MODEM_FLAG, CHIP_FLAG)
	{
	}

	void OnRadioInterrupt(const uint32_t timestamp)
	{
		switch (State)
		{
		case StateEnum::Hop:
		case StateEnum::HopStartRx:
			RxTimestamp = micros();
			State = StateEnum::PendingRxInterrupt;
			InterruptErrors |= (uint16_t)RadioErrorCodeEnum::InterruptWhileHop;
			TS::Task::enable();
			break;
		case StateEnum::WaitingForRxInterrupt:
			RxTimestamp = micros();
			State = StateEnum::PendingRxInterrupt;
			TS::Task::enable();
			break;
		case StateEnum::WaitingForTxInterrupt:
			State = StateEnum::PendingTxInterrupt;
			TS::Task::enable();
			break;
		case StateEnum::PendingRxInterrupt:
		case StateEnum::RxDelivery:
			InterruptErrors |= (uint16_t)RadioErrorCodeEnum::InterruptWhileRx;
			break;
		case StateEnum::PendingTxInterrupt:
		case StateEnum::TxReady:
			InterruptErrors |= (uint16_t)RadioErrorCodeEnum::InterruptWhileTx;
			break;
		case StateEnum::Disabled:
		default:
			break;
		}
	}

protected:
	void RadioSetup(void (*onRadioInterrupt)(void))
	{
		RadioInterrupt = onRadioInterrupt;
	}

	const bool SetTxPower(const uint8_t txPower, const uint32_t timeoutMicros = 0)
	{
		return SpiDriver.SetTxPower(txPower, timeoutMicros);
	}

	const bool RadioStart(const uint8_t* configuration, const size_t configurationSize, const uint8_t* patch = nullptr, size_t patchSize = 0)
	{
		RadioStop();
		pinMode(pinInterrupt, INPUT_PULLUP);

		if (RadioInterrupt == nullptr
			|| !SpiDriver.Start(configuration, configurationSize, patch, patchSize))
		{
			return false;
		}

		// Start listening to IRQ.
		attachInterrupt(digitalPinToInterrupt(pinInterrupt), RadioInterrupt, FALLING);

		HopChannel = 0;
		State = StateEnum::Hop;

		TS::Task::enable();

		return true;
	}

	void RadioStop()
	{
		SpiDriver.Stop();
		detachInterrupt(digitalPinToInterrupt(pinInterrupt));
		State = StateEnum::Disabled;
	}

	void RadioRx(const uint8_t channel)
	{
		switch (State)
		{
		case StateEnum::Hop:
		case StateEnum::HopStartRx:
		case StateEnum::PendingRxInterrupt:
		case StateEnum::RxDelivery:
			HopChannel = channel;
			break;
		case StateEnum::WaitingForTxInterrupt:
			OnRadioError(RadioErrorCodeEnum::RxWhileTx);
		case StateEnum::PendingTxInterrupt:
		case StateEnum::TxReady:
			State = StateEnum::Hop;
			TS::Task::enable();
			break;
		case StateEnum::WaitingForRxInterrupt:
			if (HopChannel != channel)
			{
				State = StateEnum::Hop;
				HopChannel = channel;
				TS::Task::enable();
			}
			break;
		case StateEnum::Disabled:
		default:
			break;
		}
	}

	/// <summary>
	/// Checks internal state and pings radio over SPI to ensure Tx is possible.
	/// </summary>
	/// <returns></returns>
	const bool RadioTxAvailable(const uint32_t timeoutMicros = 50)
	{
		switch (State)
		{
		case StateEnum::Hop:
		case StateEnum::HopStartRx:
		case StateEnum::WaitingForRxInterrupt:
		case StateEnum::TxReady:
			switch (GetRadioStateFast())
			{
			case RadioStateEnum::READY:
			case RadioStateEnum::READY2:
			case RadioStateEnum::RX:
			case RadioStateEnum::SLEEP:
			case RadioStateEnum::SPI_ACTIVE:
			case RadioStateEnum::TX_TUNE:
				return true;
			case RadioStateEnum::TX:
			case RadioStateEnum::RX_TUNE:
			default:
				break;
			}
			break;
		case StateEnum::WaitingForTxInterrupt:
		case StateEnum::PendingRxInterrupt:
		case StateEnum::PendingTxInterrupt:
		case StateEnum::Disabled:
		default:
			break;
		}
		return false;
	}

	/// <summary>
	/// TxAvailable() must be called immediatelly before.
	/// </summary>
	const bool RadioTx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		const uint32_t txStart = micros();

		switch (State)
		{
		case StateEnum::Hop:
		case StateEnum::HopStartRx:
		case StateEnum::WaitingForRxInterrupt:
		case StateEnum::TxReady:
			break;
		case StateEnum::WaitingForTxInterrupt:
		case StateEnum::PendingRxInterrupt:
		case StateEnum::PendingTxInterrupt:
		case StateEnum::Disabled:
		default:
			return false;
			break;
		}

		static constexpr RadioStateEnum txState = RadioStateEnum::READY;

		if (!SpiDriver.SetRadioState(txState, 100))
		{
			return false;
		}

		HopChannel = channel;
		State = StateEnum::TxReady;

		if (!SpiDriver.ClearInterrupts(100))
		{
			return false;
		}

		if (!SpiDriver.RadioTxWrite(data, packetSize, 100))
		{
			return false;
		}

		if (!SpiDriver.SetPacketSize(packetSize, 100))
		{
			return false;
		}

		State = StateEnum::WaitingForTxInterrupt;
		if (!SpiDriver.RadioStartTx(channel, 100))
		{
			State = StateEnum::TxReady;
			return false;
		}

		TxTimestamp = micros();
		TS::Task::enable();

		return true;
	}

public:
	virtual bool Callback() final
	{
		if (InterruptErrors != 0)
		{
			NotifyInterruptErrors();
			InterruptErrors = 0;
		}

		switch (State)
		{
		case StateEnum::PendingRxInterrupt:
			if (PacketHandlerInterrupts::PacketReceived(GetPacketHandlerInterruptsFast()))
			{
				RxRssi = GetRssiLatchFast();
				State = StateEnum::RxDelivery;
			}
			break;
		case StateEnum::RxDelivery:
			State = StateEnum::Hop; // Restore to Rx on the same channel by default.
			if (SpiDriver.GetRxFifoCount(RxSize, 100)
				&& RxSize > 0 && RxSize <= MaxRxSize
				&& SpiDriver.GetRxFifo(InBuffer, RxSize, 100))
			{
				OnRxReady(InBuffer, RxTimestamp, RxSize, RxRssi);
			}
			else
			{
				OnRadioError(RadioErrorCodeEnum::RxReadError);
			}
			break;
		case StateEnum::WaitingForTxInterrupt:
			if (micros() - TxTimestamp > 20000)
			{
				State = StateEnum::TxReady;
				if (!SpiDriver.ClearInterrupts(100)
					|| !SpiDriver.ClearTxFifo(100))
				{
					OnRadioError(RadioErrorCodeEnum::TxRestoreError);
				}

				OnRadioError(RadioErrorCodeEnum::TxTimeout);
				OnTxDone();
			}
			break;
		case StateEnum::PendingTxInterrupt:
			State = StateEnum::TxReady;
			if (PacketHandlerInterrupts::PacketSent(GetPacketHandlerInterruptsFast()))
			{
				if (!SpiDriver.ClearInterrupts(100)
					|| !SpiDriver.ClearTxFifo(100))
				{
					OnRadioError(RadioErrorCodeEnum::TxRestoreError);
				}
			}
			else
			{
				if (!SpiDriver.ClearInterrupts(100)
					|| !SpiDriver.ClearTxFifo(100))
				{
					OnRadioError(RadioErrorCodeEnum::TxRestoreError);
				}
				OnRadioError(RadioErrorCodeEnum::InterruptTxFailed);
			}
			OnTxDone();
			break;
		case StateEnum::Hop:
			if (SpiDriver.SetRadioState(RadioStateEnum::READY, 100)
				&& SpiDriver.ClearInterrupts(100))
			{
				State = StateEnum::HopStartRx;
			}
			else
			{
				SpiDriver.ClearInterrupts(200);
				SpiDriver.ClearRxTxFifo(200);
				SpiDriver.SetPacketSize(MaxRxSize, 200);
				OnRadioError(RadioErrorCodeEnum::RxRestoreError);
			}
			break;
		case StateEnum::HopStartRx:
			if (SpiDriver.ClearRxFifo(100)
				&& SpiDriver.SetPacketSize(MaxRxSize, 100)
				&& SpiDriver.RadioStartRx(HopChannel, 100))
			{
				if (State == StateEnum::HopStartRx)
				{
					State = StateEnum::WaitingForRxInterrupt;
				}
			}
			break;
		case StateEnum::TxReady:
		case StateEnum::WaitingForRxInterrupt:
		case StateEnum::Disabled:
		default:
			// Nothing to do.
			TS::Task::disable();
			break;
		}

		return true;
	}

	/// <summary>
	/// Fast Read Registers.
	/// </summary>
private:
	const uint8_t GetPacketHandlerInterruptsFast()
	{
		return SpiDriver.GetFrr((uint8_t)FrrEnum::PacketHandlerInterrupts);
	}

	const uint8_t GetModemInterruptsFast()
	{
		return SpiDriver.GetFrr((uint8_t)FrrEnum::ModemInterrupts);
	}

	const uint8_t GetRssiLatchFast()
	{
		return SpiDriver.GetFrr((uint8_t)FrrEnum::LatchedRssi);
	}

	const RadioStateEnum GetRadioStateFast()
	{
		return (RadioStateEnum)SpiDriver.GetFrr((uint8_t)FrrEnum::CurrentState);
	}

	void NotifyInterruptErrors()
	{
		if (InterruptErrors & (uint16_t)RadioErrorCodeEnum::InterruptWhileHop)
		{
			OnRadioError(RadioErrorCodeEnum::InterruptWhileHop);
		}

		if (InterruptErrors & (uint16_t)RadioErrorCodeEnum::InterruptWhileRx)
		{
			OnRadioError(RadioErrorCodeEnum::InterruptWhileRx);
		}

		if (InterruptErrors & (uint16_t)RadioErrorCodeEnum::InterruptWhileTx)
		{
			OnRadioError(RadioErrorCodeEnum::InterruptWhileTx);
		}
	}
};
#endif