// LoLaCryptoPrimitives.h

#ifndef _LOLA_CRYPTO_PRIMITIVES_h
#define _LOLA_CRYPTO_PRIMITIVES_h

/*
* https://github.com/rweather/arduinolibs
*/
#include <HKDF.h>
#include <BLAKE2s.h>
#include <Poly1305.h>
#include <ChaCha.h>

/*
* https://github.com/kmackay/micro-ecc
*/ 
#include <uECC.h>


class LoLaCryptoPrimitives
{
public:
	using CypherType = ChaCha;
	using MacHashType = Poly1305;
	using KeyHashType = BLAKE2s;
};
#endif