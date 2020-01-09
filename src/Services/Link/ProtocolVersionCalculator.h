// ProtocolVersionCalculator.h

#ifndef _PROTOCOLVERSIONCALCULATOR_h
#define _PROTOCOLVERSIONCALCULATOR_h

#include <Services\Link\LoLaLinkDefinitions.h>

#include <FastCRC.h>

class ProtocolVersionCalculator
{
private:
	FastCRC32 Hasher;

	bool Cached = false;
	uint16_t VersionCode = 0;

public:
	ProtocolVersionCalculator() : Hasher()
	{

	}

	uint32_t GetCode()
	{
		if (!Cached)
		{
			Cached = true;

			uint8_t Helper[8];

			// Clear hash.
			Helper[0] = 0;
			Hasher.crc32(Helper, 1);

			// Cypher.
			Helper[0] = 'A';
			Helper[1] = 'S';
			Helper[2] = 'C';
			Helper[3] = 'O';
			Helper[4] = 'N';
			Helper[5] = '1';
			Helper[6] = '2';
			Helper[7] = '8';
			Hasher.crc32_upd(Helper, 8);

			// MAC-CRC.
			Helper[0] = 'B';
			Helper[1] = 'L';
			Helper[2] = 'A';
			Helper[3] = 'K';
			Helper[4] = 'E';
			Helper[5] = '2';
			Helper[6] = 'S';

			Hasher.crc32_upd(Helper, 7);

#ifdef LOLA_LINK_USE_TOKEN_HOP
			Helper[0] = 1;
			Hasher.crc32_upd(Helper, 1);
#endif

#ifdef LOLA_LINK_USE_CHANNEL_HOP
			Helper[0] = 2;
			Hasher.crc32_upd(Helper, 1);
#endif

			Helper[0] = PACKET_DEFINITION_ACK_HEADER;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = PACKET_DEFINITION_LINK_START_HEADER;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_PACKET_MACCRC_INDEX;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_PACKET_MACCRC_SIZE;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_PACKET_HEADER_INDEX;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_PACKET_ID_INDEX;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_PACKET_PAYLOAD_INDEX;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_PACKET_MIN_PACKET_SIZE;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_PACKET_MAX_PACKET_SIZE;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_LINK_CRYPTO_SEED_SIZE;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_LINK_CHANNEL_SEED_SIZE;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_LINK_CRYPTO_TOKEN_SIZE;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_LINK_CHANNEL_TOKEN_SIZE;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_LINK_SERVICE_LINKED_TOKEN_HOP_PERIOD_MILLIS;
			Hasher.crc32_upd(Helper, 1);

			Helper[0] = LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS;
			VersionCode = Hasher.crc32_upd(Helper, 1);

			//Helper[0] = ;
			//Hasher.crc32_upd(Helper, 1);

		}

		return VersionCode;
	}
};
#endif

