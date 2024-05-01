// XoodyakHashWrapper.h

#ifndef _XOODYAK_HASH_WRAPPER_h
#define _XOODYAK_HASH_WRAPPER_h

/*
* https://github.com/rweather/lightweight-crypto
*/
#include "lightweight-crypto\xoodyak.h"
#include <stdint.h>

template<const uint8_t MacSize>
class XoodyakHashWrapper final
{
public:
	static constexpr uint8_t DIGEST_LENGTH = XOODYAK_HASH_SIZE;

private:
	static constexpr uint8_t MATCH_MIN_SIZE = sizeof(uint32_t);

private:
	xoodyak_hash_state_t State{};

	static constexpr uint8_t GetMatchSize()
	{
		return ((MacSize >= MATCH_MIN_SIZE) * MacSize) + ((MacSize < MATCH_MIN_SIZE) * MATCH_MIN_SIZE);
	}

	uint8_t Match[GetMatchSize()]{};

public:
	void reset()
	{
		xoodyak_hash_init(&State);
	}

	void update(const uint16_t value)
	{
		Match[0] = value;
		Match[1] = value >> 8;
		xoodyak_hash_absorb(&State, Match, sizeof(uint16_t));
	}

	void update(const uint32_t value)
	{
		Match[0] = value;
		Match[1] = value >> 8;
		Match[2] = value >> 16;
		Match[3] = value >> 24;
		xoodyak_hash_absorb(&State, Match, sizeof(uint32_t));
	}

	void update(const uint8_t value)
	{
		Match[0] = value;
		xoodyak_hash_absorb(&State, Match, sizeof(uint8_t));
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
		xoodyak_hash_squeeze(&State, Match, MacSize);
		for (uint_fast8_t i = 0; i < MacSize; i++)
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