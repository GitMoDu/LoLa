// ArduinoEntropy.H
#ifndef _ARDUINO_ENTROPY_h
#define _ARDUINO_ENTROPY_h

#include "IEntropy.h"
#include <Arduino.h>

/// <summary>
/// Low quality entropy, based on fixed "noise" and Arduino Random implementation.
/// </summary>
class ArduinoLowEntropy final : public virtual IEntropy
{
private:
	const uint8_t ExtraNoise;

public:
	ArduinoLowEntropy(const uint8_t extraNoise = 0)
		: IEntropy()
		, ExtraNoise(extraNoise)
	{}

	virtual const uint32_t GetNoise() final
	{
		return (((uint32_t)random(INT32_MAX)) ^ random(INT32_MAX)) + ~ExtraNoise;
	}

	virtual const uint8_t* GetUniqueId(uint8_t& idSize) final
	{
		idSize = sizeof(uint8_t);

		return &ExtraNoise;
	}
};

/// <summary>
/// Uses a (disconnected) Pin as an analog entropy source.
/// </summary>
/// <typeparam name="EntropyPin">Analog input pin number.</typeparam>
template<const uint8_t EntropyPin>
class ArduinoPinEntropy final : public Abstract1BitEntropy
{
private:
	static constexpr uint8_t NOISE_CAPTURE_STEPS = sizeof(uint32_t) * 8;

private:
	const uint8_t ExtraNoise;

public:
	ArduinoPinEntropy(const uint8_t extraNoise = 0)
		: Abstract1BitEntropy()
		, ExtraNoise(extraNoise)
	{}

	virtual const uint8_t* GetUniqueId(uint8_t& idSize) final
	{
		idSize = sizeof(uint8_t);

		return &ExtraNoise;
	}

protected:
	virtual void OpenEntropy() final
	{
#if defined(INPUT_ANALOG)
		pinMode(EntropyPin, INPUT_ANALOG);
#else
		pinMode(EntropyPin, INPUT);
#endif
	}

	virtual void CloseEntropy() final
	{}

	/// <summary>
	/// The ADC's least significant bit is the source of entropy.
	/// </summary>
	/// <returns>1 bit of entropy.</returns>
	virtual bool Get1BitEntropy() final
	{
		return (analogRead(EntropyPin) & 0x01) > 0;
	}
};
#endif