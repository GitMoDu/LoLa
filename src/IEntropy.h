// IEntropy.h
#ifndef _I_ENTROPY_h
#define _I_ENTROPY_h

#include <stdint.h>

/// <summary>
/// Interface for entropy sourcing.
/// </summary>
class IEntropy
{
public:
	virtual const uint8_t* GetUniqueId(uint8_t& idSize)
	{
		idSize = 0;
		return nullptr;
	}

	virtual const uint32_t GetNoise() { return 0; }
};
#endif