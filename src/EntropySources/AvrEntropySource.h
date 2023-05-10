// AvrEntropySource.H

#ifndef _AVR_ENTROPY_SOURCE_h
#define _AVR_ENTROPY_SOURCE_h

#include <IEntropySource.h>
#include <Arduino.h>

/// <summary>
/// AVRs don't have a unique serial id, so a Device Signature is the second best option.
/// The signture should be different for each deployed device.
/// </summary>
class AvrEntropySource : public virtual IEntropySource
{
public:
	AvrEntropySource() : IEntropySource()
	{}

	virtual const uint8_t* GetUniqueId(uint8_t& idSize) final
	{
		// https://microchipsupport.force.com/s/article/How-to-read-signature-byte
		idSize = 0;
		return nullptr;
	}

	virtual const uint32_t GetNoise() final
	{ 
		SetupInternalSensor();

		return ADCL;
	}

private:
	void SetupInternalSensor()
	{
		// Set the internal reference and mux.
		ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
		ADCSRA |= _BV(ADEN);  // enable the ADC

		ADCSRA |= _BV(ADSC);  // Start the ADC

		// Detect end-of-conversion
		while (bit_is_set(ADCSRA, ADSC));
	}
};
#endif