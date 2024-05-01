// Si446xAbstractTransceiver.h

#ifndef _SI446X_ABSTRACT_TRANSCEIVER_h
#define _SI446X_ABSTRACT_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>
#include "../SpiDriver/Si446xRadioDriver.h"

/// <summary>
/// 
/// </summary>
/// <typeparam name="RadioConfig"></typeparam>
/// <typeparam name="pinCS"></typeparam>
/// <typeparam name="pinSDN"></typeparam>
/// <typeparam name="pinInterrupt"></typeparam>
/// <typeparam name="pinCLK"></typeparam>
/// <typeparam name="pinMISO"></typeparam>
/// <typeparam name="pinMOSI"></typeparam>
/// <typeparam name="spiChannel"></typeparam>
template<typename RadioConfig,
	const uint8_t pinCS,
	const uint8_t pinSDN,
	const uint8_t pinInterrupt,
	const uint8_t pinCLK = UINT8_MAX,
	const uint8_t pinMISO = UINT8_MAX,
	const uint8_t pinMOSI = UINT8_MAX,
	const uint8_t spiChannel = 0>
class Si446xAbstractTransceiver
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
private:
	using BaseClass = Si446xRadioDriver<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE, pinCS, pinSDN, pinInterrupt, pinCLK, pinMISO, pinMOSI, spiChannel>;

	static constexpr uint32_t TRANSCEIVER_CODE = (uint32_t)RadioConfig::TRANSCEIVER_ID << 16
		| (uint32_t)RadioConfig::ChannelCount << 8
		| ((uint32_t)(RadioConfig::BaseFrequency / 10000000) + (uint32_t)(RadioConfig::BitRate / 10000));

private:
	ILoLaTransceiverListener* Listener = nullptr;

protected:
	virtual const uint8_t* GetConfigurationArray(size_t& configurationSize) { return nullptr; }

public:
	Si446xAbstractTransceiver(Scheduler& scheduler)
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
			Listener->OnRx(data, timestamp - GetRxShift(packetSize), packetSize, GetNormalizedRssi(rssiLatch));
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
		const uint8_t* configuration = nullptr;
		size_t configurationSize = 0;

		configuration = GetConfigurationArray(configurationSize);

		if (configuration == nullptr
			|| configurationSize == 0)
		{
			return false;
		}

		if (Listener != nullptr
			&& BaseClass::RadioStart(configuration, configurationSize))
		{
			return BaseClass::SetTxPower(RadioConfig::TxPowerMax);
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
		return TRANSCEIVER_CODE;
	}

	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
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
		return GetDurationInAirInternal(packetSize);
	}

private:
	/// </summary>
	/// Converts the LoLa abstract channel into a real channel index for the radio.
	/// </summary>
	/// <param name="abstractChannel">LoLa abstract channel [0;255].</param>
	/// <returns>Returns the real channel to use [0;(ChannelCount-1)].</returns>
	static constexpr uint8_t GetRawChannel(const uint8_t abstractChannel)
	{
		return ILoLaTransceiver::GetRealChannel<RadioConfig::ChannelCount>(abstractChannel);
	}

	/// <summary>
	/// TODO: Add response curve, since rssiLatch is logarithmic.
	/// </summary>
	/// <param name="rssiLatch"></param>
	/// <returns></returns>
	static const uint8_t GetNormalizedRssi(const uint8_t rssiLatch)
	{
		if (rssiLatch < RadioConfig::RSSI_LATCH_MIN)
		{
			return 0;
		}
		else if (rssiLatch >= RadioConfig::RSSI_LATCH_MAX)
		{
			return UINT8_MAX;
		}
		else
		{
			return ((uint16_t)(rssiLatch - RadioConfig::RSSI_LATCH_MIN) * UINT8_MAX) / (RadioConfig::RSSI_LATCH_MAX - RadioConfig::RSSI_LATCH_MIN);
		}
	}

	static constexpr uint8_t GetRawTxPower(const uint8_t abstractTxPower)
	{
		return RadioConfig::TxPowerMin + (uint8_t)(((uint16_t)abstractTxPower * (RadioConfig::TxPowerMax - RadioConfig::TxPowerMin)) / UINT8_MAX);
	}

	static constexpr uint16_t GetTimeToAirInternal(const uint8_t packetSize)
	{
		return RadioConfig::TTA_SHORT + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * (RadioConfig::TTA_LONG - RadioConfig::TTA_SHORT)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	}

	static constexpr uint16_t GetDurationInAirInternal(const uint8_t packetSize)
	{
		return RadioConfig::DIA_SHORT + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * (RadioConfig::DIA_LONG - RadioConfig::DIA_SHORT)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	}

	static constexpr uint16_t GetRxOffset(const uint8_t packetSize)
	{
		return RadioConfig::RX_OFFSET_SHORT + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * (RadioConfig::RX_OFFSET_LONG - RadioConfig::RX_OFFSET_SHORT)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
	}

	static constexpr uint16_t GetRxShift(const uint8_t packetSize)
	{
		return GetDurationInAirInternal(packetSize) + GetRxOffset(packetSize);
	}
};
#endif