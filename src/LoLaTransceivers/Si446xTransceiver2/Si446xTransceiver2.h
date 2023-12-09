// Si446xTransceiver2.h

#ifndef _LOLA_SI446X_TRANSCEIVER2_h
#define _LOLA_SI446X_TRANSCEIVER2_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <ILoLaTransceiver.h>

#include "SpiDriver\LoLaSi446xRadioTask.h"
#include <Si446x.h>


/// <summary>
/// 
/// </summary>
/// <typeparam name="CsPin"></typeparam>
/// <typeparam name="SdnPin"></typeparam>
/// <typeparam name="InterruptPin"></typeparam>
template<const uint8_t CsPin,
	const uint8_t SdnPin,
	const uint8_t InterruptPin>
class Si446xTransceiver2 final
	: public LoLaSi446xRadioTask<
	LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE,
	CsPin,
	SdnPin,
	InterruptPin>
	, public virtual ILoLaTransceiver
{
	using BaseClass = LoLaSi446xRadioTask<LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE, CsPin, SdnPin, InterruptPin>;

	static constexpr uint8_t ChannelCount = 10;//TODO: LoLaSi4463Config::GetRadioConfigChannelCount(MODEM_2_1);

	static constexpr uint16_t TRANSCEIVER_ID = 0x4463;

private:
	// TODO: These values might change with different SPI and CPU clock speed.
	static constexpr uint16_t TTA_LONG = 719;
	static constexpr uint16_t TTA_SHORT = 638;

	static constexpr uint16_t DIA_LONG = 1923;
	static constexpr uint16_t DIA_SHORT = 987;

private:
	ILoLaTransceiverListener* Listener = nullptr;

public:
	Si446xTransceiver2(Scheduler& scheduler, SPIClass* spiInstance)
		: ILoLaTransceiver()
		, BaseClass(scheduler, spiInstance)
	{
	}

	void SetupInterrupt(void (*onRadioInterrupt)(void))
	{
		BaseClass::RadioSetup(onRadioInterrupt);
	}

	void OnRadioInterrupt()
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
		if (packetSize >= LoLaPacketDefinition::MIN_PACKET_SIZE
			|| packetSize <= LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
		{
			Listener->OnRx(data, timestamp, packetSize, GetNormalizedRssi(rssiLatch));
		}
	}

	virtual void OnWakeUpTimer() final
	{
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
		if (Listener != nullptr
			&& BaseClass::RadioStart())
		{
			Serial.println(F("Si446x Transceiver Ok."));
			//TODO: Get DeviceId and set MaxTxPower accordingly.


			//SpiDriver.SetRadioTransmitPower(DEFAULT_POWER);
			//Si446x_setTxPower(DEFAULT_POWER);

			return true;
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
		return BaseClass::RadioTxAvailable();
	}

	virtual const uint32_t GetTransceiverCode() final
	{
		return (uint32_t)TRANSCEIVER_ID
			//| (uint32_t)LoLaSi4463Config::GetConfigId(MODEM_2_1) << 16
			| (uint32_t)0x23 << 16
			| (uint32_t)ChannelCount << 24;
	}

	/// <summary>
	/// Packet copy to serial buffer, and start transmission.
	/// The first byte is the packet size, excluding the size byte.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="length"></param>
	/// <returns>True if transmission was successful.</returns> 
	virtual const bool Tx(const uint8_t* data, const uint8_t packetSize, const uint8_t channel)
	{
#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, HIGH);
#endif
		if (TxAvailable())
		{
			const uint8_t realChannel = GetRealChannel(channel);

			return BaseClass::RadioTx(data, packetSize, realChannel);
		}
#ifdef TX_TEST_PIN
		digitalWrite(TX_TEST_PIN, LOW);
#endif
		return false;
	}

	/// <summary>
	/// </summary>
	/// <param name="channel">LoLa abstract channel [0;255].</param>
	virtual void Rx(const uint8_t channel) final
	{
		const uint8_t realChannel = GetRealChannel(channel);

		BaseClass::RadioRx(realChannel);
	}

	virtual const uint16_t GetTimeToAir(const uint8_t packetSize) final
	{
		if (packetSize < LoLaPacketDefinition::MIN_PACKET_SIZE)
		{
			return TTA_SHORT;
		}
		else
		{
			return TTA_SHORT + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * (TTA_LONG - TTA_SHORT)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
		}
	}

	virtual const uint16_t GetDurationInAir(const uint8_t packetSize) final
	{
		if (packetSize < LoLaPacketDefinition::MIN_PACKET_SIZE)
		{
			return DIA_SHORT;
		}
		else
		{
			return DIA_SHORT + ((((uint32_t)(packetSize - LoLaPacketDefinition::MIN_PACKET_SIZE)) * (DIA_LONG - DIA_SHORT)) / LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE);
		}
	}

private:
	/// </summary>
	/// Converts the LoLa abstract channel into a real channel index for the radio.
	/// </summary>
	/// <param name="abstractChannel">LoLa abstract channel [0;255].</param>
	/// <returns>Returns the real channel to use [0;(ChannelCount-1)].</returns>
	static constexpr uint8_t GetRealChannel(const uint8_t abstractChannel)
	{
		return TransceiverHelper<ChannelCount>::GetRealChannel(abstractChannel);
	}

	const uint8_t GetNormalizedRssi(const uint8_t rssiLatch)
	{
		return (UINT8_MAX - rssiLatch);
	}
};
#endif