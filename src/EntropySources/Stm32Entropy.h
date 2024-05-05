// Stm32Entropy.h

#ifndef _STM32_ENTROPY_h
#define _STM32_ENTROPY_h

#include "IEntropy.h"
#include <libmaple/adc.h>

/// <summary>
/// Uses the internal temperature sensor as an analog entropy source.
/// STM32 unique ID is read straight from the IC.
/// TODO: Use shortest possible ADC sampling period.
/// </summary>
class Stm32Entropy final : public Abstract1BitEntropy
{
private:
	static constexpr uint32_t ID_ADDRESS_POINTER = 0x1FFFF7E8;
	static constexpr uint8_t STM32_ID_LENGTH = 12;
	static constexpr uint8_t ADC_CHANNEL_TEMPERATURE = 16;

public:
	Stm32Entropy() : Abstract1BitEntropy()
	{}

	virtual const uint8_t* GetUniqueId(uint8_t& idSize) final
	{
		idSize = STM32_ID_LENGTH;

		return (uint8_t*)ID_ADDRESS_POINTER;
	}

protected:
	/// <summary>
	/// Follows example ATempSensor.
	/// </summary>
	virtual void OpenEntropy() final
	{
		// Set the TSVREFE bit in the ADC control register 2 (ADC_CR2).
		// Wakes up the temperature sensor from power down mode.
		ADC1->regs->CR2 |= ADC_CR2_TSVREFE;

		// Select a short sample time of a few cycles.
		ADC1->regs->SMPR1 = (0b111 << (3 * 6));

		// According to the Datasheet section 5.3.21 it takes from 4 to 10 uS to power up the sensor.
		delayMicroseconds(10);
	}

	virtual void CloseEntropy() final
	{
		// Clear the TSVREFE bit in the ADC control register 2 (ADC_CR2).
		// Return the temperature sensor to power down mode.
		ADC1->regs->CR2 &= ~ADC_CR2_TSVREFE;
	}

	/// <summary>
	/// The ADC's least significant bit is the source of entropy.
	/// </summary>
	/// <returns>1 bit of entropy.</returns>
	virtual bool Get1BitEntropy() final
	{
		return (adc_read(ADC1, ADC_CHANNEL_TEMPERATURE) & 0x01) > 0;
	}
};
#endif