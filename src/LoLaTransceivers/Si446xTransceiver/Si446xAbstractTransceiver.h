// Si446xAbstractTransceiver.h

#ifndef _SI446X_ABSTRACT_TRANSCEIVER_h
#define _SI446X_ABSTRACT_TRANSCEIVER_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include "../ILoLaTransceiver.h"
#include "SpiDriver/Si446xRadioDriver.h"

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

	static constexpr uint8_t CalibrationSamples = 5;


private:
	ILoLaTransceiverListener* Listener = nullptr;

protected:
	virtual const uint8_t* GetPatchArray(size_t& size) { return nullptr; }
	virtual const uint8_t* GetConfigurationArray(size_t& size) { return nullptr; }

private:
	uint16_t TxDurationBase = 1;
	uint16_t TxDurationRange = 1;

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

	virtual void OnRxReady(const uint8_t* data, const uint32_t timestamp, const uint8_t packetSize, const uint8_t rssi) final
	{
		Listener->OnRx(data, timestamp - GetDurationInAir(packetSize), packetSize, GetNormalizedRssi(rssi));
	}

	virtual void OnRadioError(const RadioErrorCodeEnum radioFailCode) final
	{
#if defined(LOLA_DEBUG)
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

		return Listener != nullptr && Setup();
	}

	virtual const bool Start() final
	{
		if (Listener == nullptr)
		{
			return false;
		}

		if (!BaseClass::SetTxPower(RadioConfig::TxPowerMax, 1000))
		{
			BaseClass::RadioStart();
			return false;
		}

		return true;
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
		return (uint32_t)RadioConfig::TRANSCEIVER_ID << 16
			| (uint32_t)RadioConfig::ChannelCount << 8
			| (uint32_t)(RadioConfig::BitRate / 10000)
			| (uint32_t)(RadioConfig::BaseFrequency / 10000000);
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
		if (packetSize >= LoLaPacketDefinition::MIN_PACKET_SIZE)
		{
			return ((uint32_t)TxDurationBase)
				+ ((((uint32_t)TxDurationRange) * (packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE))
					/ (LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE - LoLaPacketDefinition::MIN_PACKET_SIZE));
		}
		else
		{
			return TxDurationBase;
		}
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		return (RadioConfig::TTRX_SHORT - TxDurationBase) + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * ((RadioConfig::TTRX_LONG - RadioConfig::TTRX_SHORT) - TxDurationRange)) / (LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE - LoLaPacketDefinition::MIN_PACKET_SIZE));
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
	const bool Setup()
	{
		const uint8_t* configuration = nullptr;
		size_t configurationSize = 0;

		const uint8_t* patch = nullptr;
		size_t patchSize = 0;

		configuration = GetConfigurationArray(configurationSize);
		patch = GetPatchArray(patchSize);

		if (configuration == nullptr
			|| configurationSize == 0)
		{
			return false;
		}

		if (!BaseClass::RadioSetup(configuration, configurationSize, patch, patchSize))
		{
			return false;
		}

		BaseClass::RadioStart();

		if (!BaseClass::SetTxPower(0, 1000)
			|| !Calibrate(20000))
		{
			return false;
		}

		if (!BaseClass::RadioSleep(200000))
		{
			return false;
		}

		return true;
	}

	const bool Calibrate(const uint32_t timeout)
	{
		uint32_t sum = 0;

		sum = 0;
		for (uint_fast8_t i = 0; i < CalibrationSamples; i++)
		{
			const uint32_t now = micros();
			uint16_t txDuration = 0;

			if (!MeasureTx(txDuration, timeout, LoLaPacketDefinition::MIN_PACKET_SIZE))
			{
				return false;
			}

			sum += txDuration;
		}

		const uint32_t txShort = sum / CalibrationSamples;

		sum = 0;
		for (uint_fast8_t i = 0; i < CalibrationSamples; i++)
		{
			const uint32_t now = micros();
			uint16_t txDuration = 0;

			if (!MeasureTx(txDuration, timeout, LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE))
			{
				return false;
			}
			sum += txDuration;
		}

		const uint32_t txLong = sum / CalibrationSamples;


		if (txShort >= RadioConfig::TTRX_SHORT
			|| txLong >= RadioConfig::TTRX_LONG
			|| txLong <= txShort)
		{
			return false;
		}

		TxDurationBase = txShort;
		TxDurationRange = txLong - txShort;

		return true;
	}

	const bool MeasureTx(uint16_t& txDuration, const uint32_t timeout, const uint8_t testSize)
	{
		uint8_t data[LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE]{};
		uint32_t start = 0;
		uint32_t end = 0;

		txDuration = 0;
		start = micros();
		while (!BaseClass::RadioTxAvailable(timeout))
		{
			if (micros() - start > timeout)
			{
				return false;
			}
			else
			{
				BaseClass::Callback();
			}
		}

		start = micros();
		const bool success = Tx(data, testSize, 0);
		end = micros();

		if (!success)
		{
			return false;
		}

		txDuration = end - start;

		return true;
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
};
#endif