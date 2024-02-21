// DiscoveryDefinitions.h

#ifndef _DISCOVERY_DEFINITIONS_h
#define _DISCOVERY_DEFINITIONS_h

#include "..\..\Link\LoLaPacketDefinition.h"

namespace DiscoveryDefinitions
{
	static constexpr uint8_t DISCOVERY_HEADER = UINT8_MAX;
	static constexpr uint8_t DISCOVERY_ID_SIZE = 4;

	/// <summary>
	/// Discovery sub header definition.
	/// User classes should extend sub-headers starting from 0, up to UINT8_MAX-1.
	/// </summary>
	struct DiscoveryDefinition : public TemplateHeaderDefinition<DISCOVERY_HEADER, 1 + DISCOVERY_ID_SIZE>
	{
		static constexpr uint8_t PAYLOAD_ACK_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_ID_INDEX = PAYLOAD_ACK_INDEX + 1;

		/// <summary>
		/// 
		/// </summary>
		/// <param name="payloadSize"></param>
		/// <returns>The largest required payload size.</returns>
		static constexpr uint8_t MaxPayloadSize(const uint8_t payloadSize)
		{
			return ((payloadSize >= PAYLOAD_SIZE) * payloadSize) + ((payloadSize < PAYLOAD_SIZE) * PAYLOAD_SIZE);
		}
	};
}
#endif