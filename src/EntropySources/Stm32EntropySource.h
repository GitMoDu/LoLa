// Stm32EntropySource.H

#ifndef _STM32_ENTROPY_SOURCE_h
#define _STM32_ENTROPY_SOURCE_h

#include <IEntropySource.h>
#include <Arduino.h>

class Stm32EntropySource : public virtual IEntropySource
{
private:
	//For STM32, unique ID is read straight from the IC.
	static constexpr uint32_t ID_ADDRESS_POINTER = 0x1FFFF7E8;
	static constexpr uint8_t STM32_ID_LENGTH = 12;

	static constexpr uint8_t NOISE_CAPTURE_STEPS = sizeof(uint32_t) * 8;

public:
	Stm32EntropySource() : IEntropySource()
	{}

	virtual const uint8_t* GetUniqueId(uint8_t& idSize) final
	{
		idSize = STM32_ID_LENGTH;

		return (uint8_t*)ID_ADDRESS_POINTER;
	}

	virtual const uint32_t GetNoise() final
	{
		SetupTemperatureSensor();

		uint32_t noise = 0;
		for (uint_least8_t i = 0; i < NOISE_CAPTURE_STEPS; i++)
		{
			noise += (adc_read(ADC1, 16) & 0b00000001) << i;
		}

		return noise;
	}

private:
	/// <summary>
	/// Follows example ATempSensor.
	/// </summary>
	void SetupTemperatureSensor()
	{
		adc_reg_map* regs = ADC1->regs;

		// 3. Set the TSVREFE bit in the ADC control register 2 (ADC_CR2) to wake up the
		//    temperature sensor from power down mode.  Do this first 'cause according to
		//    the Datasheet section 5.3.21 it takes from 4 to 10 uS to power up the sensor.

		regs->CR2 |= ADC_CR2_TSVREFE;

		// 2. Select a sample time of 17.1 μs
		// set channel 16 sample time to 239.5 cycles
		// 239.5 cycles of the ADC clock (72MHz/6=12MHz) is over 17.1us (about 20us), but no smaller
		// sample time exceeds 17.1us.

		regs->SMPR1 = (0b111 << (3 * 6));     // set channel 16, the temp. sensor
	}
};

#endif