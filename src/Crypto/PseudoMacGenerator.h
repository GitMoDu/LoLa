// PseudoMacGenerator.h

#ifndef _PSEUDOMACGENERATOR_h
#define _PSEUDOMACGENERATOR_h

#include <stdint.h>
#include <Crypto\TinyCRC.h>

#define LOLA_INVALID_PMAC						UINT32_MAX

//#if defined(ARDUINO_ARCH_STM32F1)
//#elif defined(ARDUINO_ARCH_AVR)


class TinyCrc32Posix : public AbstractTinyCrc<uint32_t>
{
	//Imported from https://github.com/bakercp/CRC32 without changing functionality.
	//Posix compatible CRC32 (Checksum) calculator.
	//Slow and low memory overhead.
	
private:
	const uint32_t crc32_table[16] PROGMEM = {
	0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
	0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
	0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
	0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};

public:
	uint32_t Update(const uint8_t value)
	{
		// via http://forum.arduino.cc/index.php?topic=91179.0
		uint8_t tbl_idx = 0;

		tbl_idx = Seed ^ (value >> (0 * 4));
		Seed = (pgm_read_dword_near((crc32_table + (tbl_idx & 0x0f)) ^ (Seed >> 4)));
		tbl_idx = Seed ^ (value >> (1 * 4));
		Seed = (pgm_read_dword_near((crc32_table + (tbl_idx & 0x0f)) ^ (Seed >> 4)));

		return Seed;
	}

	uint32_t Reset()
	{
		Seed = ~0L;

		return Seed;
	}

};

class PseudoMacGenerator
{
private:
	uint32_t CachedPMAC = LOLA_INVALID_PMAC;
	uint8_t* IdPointer = nullptr;
	uint8_t IdLength = 0;

	TinyCrc32Posix Hasher;

public:
	PseudoMacGenerator()
	{
	}

	bool SetIdSource(uint8_t *idPointer, const uint8_t idLength) 
	{

	}

	uint32_t GetPMAC()
	{
		UpdateCached();

		return CachedPMAC;
	}

private:
	bool UpdateCached()
	{
		if (IdPointer != nullptr && IdLength > 0)
		{
			if (CachedPMAC == LOLA_INVALID_PMAC)
			{
				Hasher.Reset();
				Hasher.Update(IdPointer, IdLength);
				CachedPMAC = Hasher.GetCurrent();
			}

			return true;
		}

		return false;
	}
};
#endif