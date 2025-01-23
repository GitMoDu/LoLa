// Si446xAbstractTransceiver.h

#ifndef _SI446X_ABSTRACT_TRANSCEIVER_h
#define _SI446X_ABSTRACT_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include "../../ILoLaTransceiver.h"
#include "../SpiDriver/Si446xRadioDriver.h"

/// <summary>
/// Abstract SI446X LoLa Transceiver.
/// Implements transceiver interface over radio driver.
/// </summary>
/// <typeparam name="RadioConfig"></typeparam>
/// <typeparam name="pinCS"></typeparam>
/// <typeparam name="pinSDN"></typeparam>
/// <typeparam name="pinInterrupt"></typeparam>
template<typename RadioConfig,
	const uint8_t pinCS,
	const uint8_t pinSDN,
	const uint8_t pinInterrupt>
class Si446xAbstractTransceiver
	: public Si446xRadioDriver<
	LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE,
	pinCS,
	pinSDN,
	pinInterrupt>
	, public virtual ILoLaTransceiver
{
private:
	using BaseClass = Si446xRadioDriver<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE, pinCS, pinSDN, pinInterrupt>;

	static constexpr uint32_t TRANSCEIVER_CODE = (uint32_t)RadioConfig::TRANSCEIVER_ID << 16
		| (uint32_t)RadioConfig::ChannelCount << 8
		| ((uint32_t)(RadioConfig::BaseFrequency / 10000000) + (uint32_t)(RadioConfig::BitRate / 10000));

private:
	ILoLaTransceiverListener* Listener = nullptr;

protected:
	virtual const uint8_t* GetPatchArray(size_t& size) { return nullptr; }
	virtual const uint8_t* GetConfigurationArray(size_t& size) { return nullptr; }

private:
	/// <summary>
	/// For debug/display purposes only.
	/// </summary>
	uint8_t CurrentChannel = 0;

public:
	Si446xAbstractTransceiver(TS::Scheduler& scheduler, SPIClass& spiInstance)
		: ILoLaTransceiver()
		, BaseClass(scheduler, spiInstance)
	{
	}

	void SetupInterrupt(void (*onRadioInterrupt)(void))
	{
		BaseClass::RadioSetup(onRadioInterrupt);
	}

	void OnRxInterrupt()
	{
		OnInterrupt();
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
		Listener->OnRx(data, timestamp - GetRxShift(packetSize), packetSize, GetNormalizedRssi(rssiLatch));
	}

	virtual void OnRadioError(const RadioErrorCodeEnum radioFailCode) final
	{
#if defined(LOLA_DEBUG) || true
		switch (radioFailCode)
		{
		case RadioErrorCodeEnum::InterruptRxFailed:
			Serial.println(F("InterruptRxFailed"));
			break;
		case RadioErrorCodeEnum::InterruptTxFailed:
			Serial.println(F("InterruptTxFailed"));
			break;
		case RadioErrorCodeEnum::InterruptWhileHop:
			Serial.println(F("InterruptWhileHop"));
			break;
		case RadioErrorCodeEnum::InterruptWhileRx:
			Serial.println(F("InterruptWhileRx"));
			break;
		case RadioErrorCodeEnum::InterruptWhileTx:
			Serial.println(F("InterruptWhileTx"));
			break;
		case RadioErrorCodeEnum::RxReadError:
			Serial.println(F("RxReadError"));
			break;
		case RadioErrorCodeEnum::RxWhileTx:
			Serial.println(F("RxWhileTx"));
			break;
		case RadioErrorCodeEnum::TxRestoreError:
			Serial.println(F("TxRestoreError"));
			break;
		case RadioErrorCodeEnum::TxTimeout:
			Serial.println(F("TxTimeout"));
			break;
		default:
			break;
		}
#endif
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

		const uint8_t* patch = nullptr;
		size_t patchSize = 0;

		configuration = GetConfigurationArray(configurationSize);
		patch = GetPatchArray(patchSize);

		if (Listener == nullptr
			|| configuration == nullptr
			|| configurationSize == 0)
		{
			return false;
		}

		if (BaseClass::RadioStart(configuration, configurationSize, patch, patchSize))
		{
			return BaseClass::SetTxPower(RadioConfig::TxPowerMax, 1000);
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
		return BaseClass::RadioTxAvailable(100);
	}

	virtual const uint32_t GetTransceiverCode() final
	{
		return TRANSCEIVER_CODE;
	}

	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel) final
	{
		CurrentChannel = GetRawChannel(channel);

		return BaseClass::RadioTx(data, packetSize, CurrentChannel);
	}

	virtual void Rx(const uint8_t channel) final
	{
		CurrentChannel = GetRawChannel(channel);

		BaseClass::RadioRx(CurrentChannel);
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		return GetTimeToAirInternal(packetSize);
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		return GetDurationInAirInternal(packetSize);
	}

	const uint8_t GetChannelCount() final
	{
		return RadioConfig::ChannelCount;
	}

	const uint8_t GetCurrentChannel() final
	{
		return CurrentChannel;
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
		if (rssiLatch < RadioConfig::RSSI_MIN)
		{
			return 0;
		}
		else if (rssiLatch >= RadioConfig::RSSI_MAX)
		{
			return UINT8_MAX;
		}
		else
		{
			return ((uint16_t)(rssiLatch - RadioConfig::RSSI_MIN) * UINT8_MAX) / (RadioConfig::RSSI_MAX - RadioConfig::RSSI_MIN);
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

	static constexpr uint16_t GetRxShift(const uint8_t packetSize)
	{
		return GetDurationInAirInternal(packetSize);
	}
};
#endif