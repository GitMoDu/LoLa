// Stm32Entropy.h

#ifndef _STM32_ENTROPY_h
#define _STM32_ENTROPY_h

#include <IEntropy.h>
#include <Arduino.h>

/// <summary>
/// Uses the internal temperature sensor as an analog entropy source.
/// </summary>
class Stm32Entropy final : public virtual IEntropy
{
private:
	//For STM32, unique ID is read straight from the IC.
	static constexpr uint32_t ID_ADDRESS_POINTER = 0x1FFFF7E8;
	static constexpr uint8_t STM32_ID_LENGTH = 12;

	static constexpr uint8_t NOISE_CAPTURE_STEPS = sizeof(uint32_t) * 8;

public:
	Stm32Entropy() : IEntropy()
	{}

	virtual const uint8_t* GetUniqueId(uint8_t& idSize) final
	{
		idSize = STM32_ID_LENGTH;

		return (uint8_t*)ID_ADDRESS_POINTER;
	}

	/// <summary>
	/// The ADCs least significant bit is the source of entropy.
	/// </summary>
	/// <returns>32 bits of entropy.</returns>
	virtual const uint32_t GetNoise() final
	{
		SetupTemperatureSensor();

		uint32_t noise = 0;
		for (uint_fast8_t i = 0; i < NOISE_CAPTURE_STEPS; i++)
		{
			noise |= ((uint32_t)(GetTemperatureAdc() & 0x01)) << i;
		}

		return noise;
	}

private:
	/// <summary>
	/// Follows example ATempSensor.
	/// </summary>
	/// <returns>16 bit reading of ADC channel 16 (temperature sensor).</returns>
	const uint16_t GetTemperatureAdc()
	{
		return adc_read(ADC1, 16);
	}

	/// <summary>
	/// Follows example ATempSensor.
	/// </summary>
	void SetupTemperatureSensor()
	{
		// 3. Set the TSVREFE bit in the ADC control register 2 (ADC_CR2) to wake up the
		//    temperature sensor from power down mode.  Do this first 'cause according to
		//    the Datasheet section 5.3.21 it takes from 4 to 10 uS to power up the sensor.
		ADC1->regs->CR2 |= ADC_CR2_TSVREFE;

		// 2. Select a sample time of 17.1 μs
		// set channel 16 sample time to 239.5 cycles
		// 239.5 cycles of the ADC clock (72MHz/6=12MHz) is over 17.1us (about 20us), but no smaller
		// sample time exceeds 17.1us.
		ADC1->regs->SMPR1 = (0b111 << (3 * 6));     // set channel 16, the temp. sensor
	}
};

#endif