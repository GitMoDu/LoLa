// ITokenSource.h

#ifndef _I_TOKEN_SOURCE_h
#define _I_TOKEN_SOURCE_h

#include <stdint.h>

class ITokenSource
{
public:
	virtual uint32_t GetTOTPPeriod() { return 0; }
	virtual void SetSeed(const uint32_t seed) {};
	virtual uint32_t GetToken(const uint32_t syncMillis) { return 0; }
};
#endif