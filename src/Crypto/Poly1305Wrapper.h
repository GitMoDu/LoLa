// Poly1305Wrapper.h

#ifndef _POLY_1305_WRAPPER_h
#define _POLY_1305_WRAPPER_h

/*
* https://github.com/OperatorFoundation/Crypto
* (Arduino fork of https://github.com/rweather/arduinolibs)
*/
#include <Poly1305.h>
#include <stdint.h>

class Poly1305Wrapper final
{
public:
	static constexpr uint8_t DIGEST_LENGTH = 16;
	static constexpr uint8_t KEY_SIZE = 16;
	static constexpr uint8_t NONCE_SIZE = 16;

private:
	Poly1305 Hasher{};
	uint8_t Match[LoLaPacketDefinition::MAC_SIZE]{};

public:
	void reset(const uint8_t key[KEY_SIZE])
	{
		Hasher.reset(key);
	}

	void update(const uint32_t value)
	{
		Match[0] = value;
		Match[1] = value >> 8;
		Match[2] = value >> 16;
		Match[3] = value >> 24;
		Hasher.update(Match, sizeof(uint32_t));
	}

	void update(const uint16_t value)
	{
		Match[0] = value;
		Match[1] = value >> 8;
		Hasher.update(Match, sizeof(uint16_t));
	}

	void update(const uint8_t value)
	{
		Match[0] = value;
		Hasher.update(Match, sizeof(uint8_t));
	}

	void update(const uint8_t* source, const uint8_t size)
	{
		Hasher.update(source, size);
	}

	void clear()
	{
		Hasher.clear();
	}

	void finalize(const uint8_t nonce[NONCE_SIZE], uint8_t* target, const uint8_t size)
	{
		Hasher.finalize(nonce, target, size);
	}

	const bool macMatches(const uint8_t nonce[NONCE_SIZE], const uint8_t* source)
	{
		Hasher.finalize(nonce, Match, LoLaPacketDefinition::MAC_SIZE);
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