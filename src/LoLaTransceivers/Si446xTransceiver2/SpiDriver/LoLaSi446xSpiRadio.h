// LoLaSi446xSpiRadio.h

#ifndef _LOLA_SI446X_SPI_RADIO_h
#define _LOLA_SI446X_SPI_RADIO_h

#include "LoLaSi446xSpiDriver.h"

#include "lola_radio_config.h"


/// <summary>
/// </summary>
/// <typeparam name="CsPin"></typeparam>
/// <typeparam name="SdnPin"></typeparam>
template<const uint8_t CsPin,
	const uint8_t SdnPin>
class LoLaSi446xSpiRadio : public LoLaSi446xSpiDriver<CsPin>
{
private:
	using BaseClass = LoLaSi446xSpiDriver<CsPin>;

	using RadioStateEnum = LoLaSi446x::RadioStateEnum;

public:
	LoLaSi446xSpiRadio(SPIClass* spiInstance)
		: BaseClass(spiInstance)
	{
		pinMode(SdnPin, INPUT);
	}

public:
	virtual void Stop()
	{
		BaseClass::Stop();
		pinMode(SdnPin, INPUT);
	}

	virtual const bool Start()
	{
		//TODO: can be much shorter? Keep SDN Active, no need to reset on each link start?
		//digitalWrite(SdnPin, LOW);
		Reset();

		if (!BaseClass::Start())
		{
			Serial.println(F("LoLaSi446xSpiDriver Failed 1."));
			return false;
		}

		if (!BaseClass::SpinWaitClearToSend(50))
		{
			return false;
		}

		LoLaSi446x::Si446xInfoStruct deviceInfo{};

		if (!BaseClass::GetPartInfo(deviceInfo, 1))
		{
			Serial.println(F("LoLaSi446xSpiDriver Failed 2."));
			return false;
		}


		switch (deviceInfo.PartId)
		{
		case LoLaSi446x::PART_NUMBER_SI4463:
			Serial.println(F("Si4463 found."));
			break;
		default:
#if defined(DEBUG_LOLA)
			Serial.println(F("Si446xSpiDriver Unknown Part Number."));
#endif
			return false;
		}


		if (!ApplyStartupConfig())
		{
			return false;
		}

		if (!BaseClass::ClearRadioEvents(1))
		{
			return false;
		}

		if (!BaseClass::SetupCallbacks(1))
		{
			return false;
		}

		if (!BaseClass::SpinWaitClearToSend(1))
		{
			return false;
		}

		// Sleep until first command.
		if (!BaseClass::SetRadioState(RadioStateEnum::SLEEP))
		{
			return false;
		}

		if (!BaseClass::SpinWaitClearToSend(1))
		{
			return false;
		}

		if (BaseClass::GetRadioState() != RadioStateEnum::SLEEP)
		{		
			return false;
		}

		if (!BaseClass::RadioStartRx(0))
		{
			return false;
		}

		delay(100);

		if (!BaseClass::SpinWaitClearToSend(1))
		{
			return false;
		}
		if (BaseClass::GetRadioState() != RadioStateEnum::RX)
		{
			return false;
		}


		return true;
	}

public:
	const uint8_t GetLatchedRssi()
	{
		return BaseClass::GetFrr(LoLaSi446x::COMMAND_READ_FRR_A);
	}

	const bool GetRxLatchedSize()
	{
		return BaseClass::GetFrr(LoLaSi446x::COMMAND_READ_RX_FIFO);
	}

	const bool ClearRxFifo()
	{
		return BaseClass::ClearFifo(LoLaSi446x::COMMAND_CLEAR_RX_FIFO);
	}

	const bool ClearTxFifo()
	{
		return BaseClass::ClearFifo(LoLaSi446x::COMMAND_CLEAR_TX_FIFO);
	}

	const bool SetPacketSize(const uint8_t packetSize)
	{
		return BaseClass::SetProperty(LoLaSi446x::CONFIG_PACKET_SIZE_SET, packetSize);
	}

private:
	/// <summary>
	/// Reset the RF chip.
	/// </summary>
	void Reset()
	{
		pinMode(SdnPin, OUTPUT);
		digitalWrite(SdnPin, HIGH);
		delay(5);
		digitalWrite(SdnPin, LOW);
	}

	const bool ApplyStartupConfig()
	{
		return false;
		//TODO: Generalize without Macros.
	/*	const uint8_t WdsConfiguration[] PROGMEM = LOLA_RADIO_CONFIGURATION_DATA_ARRAY;

		if (!BaseClass::SpinWaitClearToSend(1))
		{
			return false;
		}

		if (!BaseClass::ApplyStartupConfig(WdsConfiguration, sizeof(WdsConfiguration), 500))
		{
			return false;
		}*/

		return true;
	}
};
#endif