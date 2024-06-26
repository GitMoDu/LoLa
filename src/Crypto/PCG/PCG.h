// PCG.h
// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
// https://www.pcg-random.org

#ifndef _PCG_h
#define _PCG_h

#include <stdint.h>

class PCG
{
public:
	struct pcg32_random_t
	{
		uint64_t state = 0;
		uint64_t inc = 0;
	};

	static const uint32_t pcg32_random_r(pcg32_random_t* rng)
	{
		uint64_t oldstate = rng->state;
		// Advance internal state
		rng->state = (oldstate * (uint64_t)6364136223846793005) + (rng->inc | 1);
		// Calculate output function (XSH RR), uses old state for max ILP
		uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
		uint32_t rot = oldstate >> 59u;
		return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
	}
};
#endif