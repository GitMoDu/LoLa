// XoodyakHashWrapper.h

#ifndef _XOODYAK_HASH_WRAPPER_h
#define _XOODYAK_HASH_WRAPPER_h

/*
* https://github.com/rweather/lightweight-crypto
*/
#include "lightweight-crypto\xoodyak.h"
#include <stdint.h>

class XoodyakHashWrapper final
{
public:
	static const uint8_t DIGEST_LENGTH = XOODYAK_HASH_SIZE;

private:
	xoodyak_hash_state_t State;

	uint8_t Match[LoLaPacketDefinition::MAC_SIZE]{};

public:
	void reset()
	{
		xoodyak_hash_init(&State);
	}

	void update(const uint8_t* source, const uint8_t size)
	{
		xoodyak_hash_absorb(&State, source, size);
	}

	void clear()
	{
		State.s.count = 0;
		State.s.mode = 0;
		for (uint_fast8_t i = 0; i < 48; i++)
		{
			State.s.state[i] = 0;
		}
	}

	void finalize(uint8_t* target, const uint8_t size)
	{
		xoodyak_hash_squeeze(&State, target, size);
	}

	const bool macMatches(const uint8_t* source)
	{
		xoodyak_hash_squeeze(&State, Match, LoLaPacketDefinition::MAC_SIZE);
		for (uint_fast8_t i = 0; i < LoLaPacketDefinition::MAC_SIZE; i++)
		{
			if (Match[i] != source[i])
			{
				return false;
			}
		}

		return true;
	}
};
#endif