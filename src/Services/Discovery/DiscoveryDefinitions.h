// DiscoveryDefinitions.h

#ifndef _DISCOVERY_DEFINITIONS_h
#define _DISCOVERY_DEFINITIONS_h

#include "../../Link/LoLaPacketDefinition.h"

namespace DiscoveryDefinitions
{
	static constexpr uint8_t DISCOVERY_HEADER = UINT8_MAX;
	static constexpr uint8_t DISCOVERY_ID_SIZE = 4;

	/// <summary>
	/// Discovery header definition.
	/// User classes should extend headers starting from 0, up to UINT8_MAX-1.
	/// SubPayload: ||META|ID0|ID1|ID2|ID3||
	/// </summary>
	struct DiscoveryDefinition : public TemplateHeaderDefinition<DISCOVERY_HEADER, 1 + DISCOVERY_ID_SIZE>
	{
		static constexpr uint8_t PAYLOAD_META_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t PAYLOAD_ID_INDEX = PAYLOAD_META_INDEX + 1;
		static constexpr uint8_t SLOT_MASK = 0b00111111;
		static constexpr uint8_t REJECT_MASK = 0b10000000;
		static constexpr uint8_t ACK_MASK = 0b01000000;

		/// <summary>
		/// Constexpr version of max(DiscoveryMaxPayload,servicePayload).
		/// </summary>
		/// <param name="payloadSize"></param>
		/// <returns>The largest required payload size.</returns>
		static constexpr uint8_t MaxPayloadSize(const uint8_t servicePayloadSize)
		{
			return ((servicePayloadSize >= PAYLOAD_SIZE) * servicePayloadSize) + ((servicePayloadSize < PAYLOAD_SIZE) * PAYLOAD_SIZE);
		}

		static constexpr bool GetAckFromMeta(const uint8_t meta)
		{
			return (meta & ACK_MASK) > 0;
		}

		static constexpr bool GetRejectFromMeta(const uint8_t meta)
		{
			return (meta & REJECT_MASK) > 0;
		}

		static constexpr uint8_t GetSlotFromMeta(const uint8_t meta)
		{
			return (uint8_t)(meta & SLOT_MASK);
		}

		static constexpr uint8_t GetMeta(const uint8_t slot, const bool reject, const bool ack)
		{
			return (slot & SLOT_MASK) | (ack * ACK_MASK) | (reject * REJECT_MASK);
		}
	};
}
#endif