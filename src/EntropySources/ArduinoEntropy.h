// ArduinoEntropy.H
#ifndef _ARDUINO_ENTROPY_h
#define _ARDUINO_ENTROPY_h

#include <IEntropy.h>
#include <Arduino.h>

/// <summary>
/// Low quality entropy, based on fixed "noise" and Arduino Random implementation.
/// </summary>
class ArduinoEntropy final : public virtual IEntropy
{
private:
	const uint8_t ExtraNoise;

public:
	ArduinoEntropy(const uint8_t extraNoise = 0) : IEntropy()
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
class ArduinoPinEntropy final : public virtual IEntropy
{
private:
	static constexpr uint8_t NOISE_CAPTURE_STEPS = sizeof(uint32_t) * 8;

private:
	const uint8_t ExtraNoise;

public:
	ArduinoPinEntropy(const uint8_t extraNoise = 0) : IEntropy()
		, ExtraNoise(extraNoise)
	{}

	/// <summary>
	/// The ADCs least significant bit is the source of entropy.
	/// </summary>
	/// <returns>32 bits of entropy.</returns>
	virtual const uint32_t GetNoise() final
	{
		pinMode(EntropyPin, INPUT_ANALOG);

		uint32_t noise = 0;

		for (uint_least8_t i = 0; i < NOISE_CAPTURE_STEPS; i++)
		{
			noise |= (analogRead(EntropyPin) & 0b00000001) << i;
		}

		return noise;
	}

	virtual const uint8_t* GetUniqueId(uint8_t& idSize) final
	{
		idSize = sizeof(uint8_t);

		return &ExtraNoise;
	}
};
#endif