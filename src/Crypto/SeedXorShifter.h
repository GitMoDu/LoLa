// SeedXorShifter.h

#ifndef _SEED_XOR_SHIFTER_h
#define _SEED_XOR_SHIFTER_h

#include <stdint.h>

/// <summary>
/// Uniform distribution output.
/// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs".
/// </summary>
class SeedXorShifter final
{
private:
	uint32_t Seed = 0;

public:
	void SetSeedArray(const uint8_t seed[sizeof(uint32_t)])
	{
		Seed = seed[0] | ((uint32_t)seed[1] << 8) | ((uint32_t)seed[2] << 16) | ((uint32_t)seed[3] << 24);
	}

	void SetSeed(const uint32_t seed)
	{
		Seed = seed;
	}

	const uint8_t GetHash(const uint32_t value)
	{
		// Initialize non-zero state with Seed and value.
		uint32_t state = Seed ^ value;
		state ^= value << 3;
		state ^= value >> 7;

		// Apply xor shift.
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;

		return state;
	}
};
#endif