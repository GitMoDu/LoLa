// AvrEntropy.h

#ifndef _AVR_ENTROPY_h
#define _AVR_ENTROPY_h

#include "IEntropy.h"
#include <Arduino.h>

/// <summary>
/// AVRs don't have a unique serial id, so a Device Signature is the second best option.
/// The signature should be different for each deployed device.
/// TODO: implement signature https://microchipsupport.force.com/s/article/How-to-read-signature-byte
/// </summary>
class AvrEntropy : public Abstract1BitEntropy
{
public:
	AvrEntropy()
		: Abstract1BitEntropy()
	{}

	virtual const uint8_t* GetUniqueId(uint8_t& idSize) final
	{

		idSize = 0;
		return nullptr;
	}

protected:
	virtual void OpenEntropy() final
	{
		// Set the internal reference and mux.
		ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
		ADCSRA |= _BV(ADEN);  // enable the ADC
	}

	virtual void CloseEntropy() final
	{
	}

	/// <summary>
	/// The ADC's least significant bit is the source of entropy.
	/// </summary>
	/// <returns>1 bit of entropy.</returns>
	virtual bool Get1BitEntropy() final
	{
		ADCSRA |= _BV(ADSC);  // Start the ADC

		// Detect end-of-conversion
		while (bit_is_set(ADCSRA, ADSC));

		return (ADCL & 0x01) > 0;
	}
};
#endif