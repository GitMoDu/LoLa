// ITokenSource.h

#ifndef _I_TOKEN_SOURCE_h
#define _I_TOKEN_SOURCE_h

#include <stdint.h>

class ITokenSource
{
public:
	virtual uint32_t GetToken() { return 0; }
};
#endif