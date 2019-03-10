// ITokenSource.h

#ifndef _I_TOKEN_SOURCE_h
#define _I_TOKEN_SOURCE_h

#include <stdint.h>

class ITokenSource
{
public:
	virtual void SetSeed(const uint32_t seed) {};
	virtual uint32_t GetToken() { return 0; }
	virtual uint32_t GetNextSwitchOverMillis() { return 0; }
};
#endif