// ArduinoEntropySource.H

#ifndef _ARDUINO_ENTROPY_SOURCE_h
#define _ARDUINO_ENTROPY_SOURCE_h

#include <IEntropySource.h>

/// <summary>
/// Low quality entropy, but it gets the job done anyways.
/// </summary>
class ArduinoEntropySource : public virtual IEntropySource
{
private:
	const uint8_t ExtraNoise;

public:
	ArduinoEntropySource(const uint8_t extraNoise = 0) : IEntropySource()
		, ExtraNoise(extraNoise)
	{}

	virtual const uint32_t GetNoise() final
	{
		return (((uint32_t)random(INT32_MAX)) ^ random(INT32_MAX)) + ExtraNoise;
	}

	//static int RNG(uint8_t* dest, unsigned size) {
	//	// Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of 
	//	// random noise). This can take a long time to generate random data if the result of analogRead(0) 
	//	// doesn't change very frequently.
	//	while (size) {
	//		uint8_t val = 0;
	//		for (unsigned i = 0; i < 8; ++i) {
	//			int init = analogRead(EntropyPin);
	//			int count = 0;
	//			while (analogRead(EntropyPin) == init) {
	//				++count;
	//			}
	//
	//			if (count == 0) {
	//				val = (val << 1) | (init & 0x01);
	//			}
	//			else {
	//				val = (val << 1) | (count & 0x01);
	//			}
	//		}
	//		*dest = val;
	//		++dest;
	//		--size;
	//	}
	//	// NOTE: it would be a good idea to hash the resulting random data using SHA-256 or similar.
	//	return 1;
	//}
};
#endif