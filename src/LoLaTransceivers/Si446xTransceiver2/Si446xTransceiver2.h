// Si446xTransceiver2.h

#ifndef _SI446X_TRANSCEIVER2_h
#define _SI446X_TRANSCEIVER2_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>

#include "SpiDriver/Si446xRadioDriver.h"
#include "SpiDriver/LoLaConfig433.h"
#include "SpiDriver/ZakKembleConfig433.h"

/// <summary>
/// 
/// </summary>
/// <typeparam name="pinCS"></typeparam>
/// <typeparam name="pinSDN"></typeparam>
/// <typeparam name="pinInterrupt"></typeparam>
/// <typeparam name="pinCLK"></typeparam>
/// <typeparam name="pinMISO"></typeparam>
/// <typeparam name="pinMOSI"></typeparam>
/// <typeparam name="spiChannel"></typeparam>
template<const uint8_t pinCS,
	const uint8_t pinSDN,
	const uint8_t pinInterrupt,
	const uint8_t pinCLK = UINT8_MAX,
	const uint8_t pinMISO = UINT8_MAX,
	const uint8_t pinMOSI = UINT8_MAX,
	const uint8_t spiChannel = 0>
class Si446xTransceiver2 final
	: public Si446xRadioDriver<
	LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE,
	pinCS,
	pinSDN,
	pinInterrupt,
	pinCLK,
	pinMISO,
	pinMOSI,
	spiChannel>
	, public virtual ILoLaTransceiver
{
	using BaseClass = Si446xRadioDriver<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE, pinCS, pinSDN, pinInterrupt, pinCLK, pinMISO, pinMOSI, spiChannel>;

	static constexpr auto ChannelCount = ZakKembleConfig433::ChannelCount;

	static constexpr uint16_t TRANSCEIVER_ID = 0x4463;

private:
	static constexpr uint16_t TTA_LONG = ZakKembleConfig433::TTA_LONG;
	static constexpr uint16_t TTA_SHORT = ZakKembleConfig433::TTA_SHORT;

	static constexpr uint16_t DIA_LONG = ZakKembleConfig433::DIA_LONG;
	static constexpr uint16_t DIA_SHORT = ZakKembleConfig433::DIA_SHORT;

	static constexpr uint8_t RSSI_LATCH_MIN = 60;
	static constexpr uint8_t RSSI_LATCH_MAX = 140;
	static constexpr uint8_t RSSI_LATCH_RANGE = RSSI_LATCH_MAX - RSSI_LATCH_MIN;

private:
	ILoLaTransceiverListener* Listener = nullptr;

public:
	Si446xTransceiver2(Scheduler& scheduler)
		: ILoLaTransceiver()
		, BaseClass(scheduler)
	{}

	void SetupInterrupt(void (*onRadioInterrupt)(void))
	{
		BaseClass::RadioSetup(onRadioInterrupt);
	}

	void OnInterrupt()
	{
		BaseClass::OnRadioInterrupt(micros());
	}

protected:
	virtual void OnTxDone() final
	{
		Listener->OnTx();
	}

	virtual void OnRxReady(const uint8_t* data, const uint32_t timestamp, const uint8_t packetSize, const uint8_t rssiLatch) final
	{
		if (packetSize >= LoLaPacketDefinition::MIN_PACKET_SIZE)
		{
			Listener->OnRx(data, timestamp - GetRxDelay(packetSize), packetSize, GetNormalizedRssi(rssiLatch));
		}
	}

public:
	// ILoLaTransceiver overrides.
	virtual const bool SetupListener(ILoLaTransceiverListener* listener) final
	{
		Listener = listener;

		return Listener != nullptr;
	}

	virtual const bool Start() final
	{
		//TODO: Use config from parameter.

		const auto configuration = ZakKembleConfig433::Config;
		const auto configurationSize = ZakKembleConfig433::ConfigSize;;

		if (Listener != nullptr
			&& BaseClass::RadioStart(configuration, configurationSize))
		{
			//TODO: Get DeviceId and set MaxTxPower accordingly.

			return BaseClass::SetTxPower(3);
		}

		return false;
	}

	virtual const bool Stop() final
	{
		BaseClass::RadioStop();

		return true;
	}

	virtual const bool TxAvailable() final
	{
		return BaseClass::RadioTxAvailable(50);
	}

	virtual const uint32_t GetTransceiverCode() final
	{
		return (uint32_t)TRANSCEIVER_ID
			//| (uint32_t)LoLaSi4463Config::GetConfigId(MODEM_2_1) << 16
			| (uint32_t)0x23 << 16
			| (uint32_t)ChannelCount << 24;
	}

	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
		return BaseClass::RadioTx(data, packetSize, GetRawChannel(channel));
	}

	virtual void Rx(const uint8_t channel) final
	{
		BaseClass::RadioRx(GetRawChannel(channel));
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return GetTimeToAirInternal(packetSize);
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		return GetDurationInAir(packetSize);
	}

private:
	/// </summary>
	/// Converts the LoLa abstract channel into a real channel index for the radio.
	/// </summary>
	/// <param name="abstractChannel">LoLa abstract channel [0;255].</param>
	/// <returns>Returns the real channel to use [0;(ChannelCount-1)].</returns>
	static constexpr uint8_t GetRawChannel(const uint8_t abstractChannel)
	{
		return GetRealChannel<ChannelCount>(abstractChannel);
	}

	static const uint8_t GetNormalizedRssi(const uint8_t rssiLatch)
	{
		if (rssiLatch < RSSI_LATCH_MIN)
		{
			return 0;
		}
		else if (rssiLatch >= RSSI_LATCH_MAX)
		{
			return UINT8_MAX;
		}
		else
		{
			return ((uint16_t)(rssiLatch - RSSI_LATCH_MIN) * UINT8_MAX) / RSSI_LATCH_RANGE;
		}
	}

	/// <summary>
	/// TODO: Calibrate.
	/// </summary>
	/// <param name="packetSize"></param>
	/// <returns></returns>
	static constexpr uint16_t GetRxDelay(const uint8_t packetSize)
	{
		return GetDurationInAirInternal(packetSize) - 50;
	}

	static constexpr uint16_t GetTimeToAirInternal(const uint8_t packetSize)
	{
		return TTA_SHORT + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * (TTA_LONG - TTA_SHORT)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	}

	static constexpr uint16_t GetDurationInAirInternal(const uint8_t packetSize)
	{
		return DIA_SHORT + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * (DIA_LONG - DIA_SHORT)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	}
};
#endif