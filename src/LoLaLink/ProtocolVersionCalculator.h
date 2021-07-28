// ProtocolVersionCalculator.h

#ifndef _PROTOCOLVERSIONCALCULATOR_h
#define _PROTOCOLVERSIONCALCULATOR_h

#include <Link\LoLaLinkDefinitions.h>
#include <LoLaDefinitions.h>
#include <Packet\PacketDefinition.h>

#include <BLAKE2s.h>

class ProtocolVersionCalculator
{
public:
	static const uint32_t GetProtocolCode()
	{
		BLAKE2s Hasher;
		uint8_t Helper[13] = {};

		Hasher.reset();

		// Key Exchange.
		Helper[0] = 'U';
		Helper[1] = 'E';
		Helper[2] = 'C';
		Helper[3] = 'C';
		Helper[4] = 'S';
		Helper[5] = 'E';
		Helper[6] = 'C';
		Helper[7] = 'P';
		Helper[8] = '1';
		Helper[9] = '6';
		Helper[10] = '0';
		Helper[11] = 'R';
		Helper[12] = '1';
		Hasher.update(Helper, 13);

		// Cypher.
		Helper[0] = 'A';
		Helper[1] = 'S';
		Helper[2] = 'C';
		Helper[3] = 'O';
		Helper[4] = 'N';
		Helper[5] = '1';
		Helper[6] = '2';
		Helper[7] = '8';
		Hasher.update(Helper, 1);

		// Hasher.
		Helper[0] = 'B';
		Helper[1] = 'L';
		Helper[2] = 'A';
		Helper[3] = 'K';
		Helper[4] = 'E';
		Helper[5] = '2';
		Helper[6] = 'S';
		Hasher.update(Helper, 7);

		// Token Generator Algorithm.
		Helper[0] = 'X';
		Helper[1] = 'O';
		Helper[2] = 'R';
		Hasher.update(Helper, 3);

		// Channel Manager PSRNG.
		Helper[0] = 'S';
		Helper[1] = 'M';
		Helper[2] = 'B';
		Helper[3] = 'U';
		Helper[4] = 'S';
		Helper[5] = '8';
		Hasher.update(Helper, 6);

		// Duplex.
		Helper[0] = ILOLA_DUPLEX_PERIOD_MILLIS;
		Hasher.update(Helper, 1);

		// Hops.
#ifdef LOLA_LINK_USE_TOKEN_HOP
		Helper[0] = LOLA_LINK_SERVICE_LINKED_TOKEN_HOP_PERIOD_SECONDS;
#else
		Helper[0] = 0;
#endif
#ifdef LOLA_LINK_USE_CHANNEL_HOP
		Helper[1] = LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS;
#else
		Helper[1] = 0;
#endif
		Hasher.update(Helper, 2);
		
		// Packet Definitions.
		Helper[0] = PacketDefinition::LOLA_PACKET_MAX_PACKET_TOTAL_SIZE;
		Helper[1] = PacketDefinition::LOLA_PACKET_MACCRC_INDEX;
		Helper[2] = PacketDefinition::LOLA_PACKET_MACCRC_SIZE;
		Helper[3] = PacketDefinition::LOLA_PACKET_ID_INDEX;
		Helper[4] = PacketDefinition::LOLA_PACKET_HEADER_INDEX;
		Helper[5] = PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX;
		Hasher.update(Helper, 6);

		// Header Definitions.
		Helper[0] = PacketDefinition::PACKET_DEFINITION_ACK_HEADER;
		Helper[1] = PacketDefinition::PACKET_DEFINITION_LINK_START_HEADER;
		Helper[2] = PacketDefinition::PACKET_DEFINITION_USER_HEADERS_START;
		Hasher.update(Helper, 3);

		union ArrayToUint32
		{
			byte array[sizeof(uint32_t)];
			uint32_t uint;
		} ATUI;

		Hasher.finalize(ATUI.array, sizeof(uint32_t));

		return ATUI.uint;
	}
};
#endif

