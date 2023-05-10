// IEntropySource.h

#ifndef _I_ENTROPY_SOURCE_h
#define _I_ENTROPY_SOURCE_h

#include <stdint.h>

class IEntropySource
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