// ISeedSource.h

#ifndef _ISEEDSOURCE_h
#define _ISEEDSOURCE_h

#include <stdint.h>

class ISeedSource
{
public:
	virtual uint8_t GetToken(const int8_t offsetMillis) { return 0; }
	virtual uint32_t GetCurrentSeed() { return 0; }
};
#endif