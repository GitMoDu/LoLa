// ISeedSource.h

#ifndef _ISEEDSOURCE_h
#define _ISEEDSOURCE_h

#include <stdint.h>

class ISeedSource
{
public:
	virtual uint8_t GetSeed() { return 0; }
};
#endif