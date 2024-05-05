// DeliveryDefinitions.h

#ifndef _DELIVERY_DEFINITIONS_h
#define _DELIVERY_DEFINITIONS_h

#include <ILinkServices.h>

struct DeliveryDefinitions
{
	static constexpr uint8_t MAX_DATA_SIZE = LoLaPacketDefinition::MAX_PAYLOAD_SIZE
		- HeaderDefinition::SUB_PAYLOAD_INDEX - 1;

	static constexpr uint8_t GetDeliveryDataSize(const uint8_t payloadSize)
	{
		return payloadSize - (DeliveryRequestDefinition::PAYLOAD_SIZE - MAX_DATA_SIZE);
	}

	static constexpr uint8_t GetDeliveryPayloadSize(const uint8_t dataSize)
	{
		return dataSize + (DeliveryRequestDefinition::PAYLOAD_SIZE - MAX_DATA_SIZE);
	}

	template<const uint8_t Header,
		const uint8_t ExtraSize = 0>
	struct DeliveryIdDefinition : public TemplateHeaderDefinition<Header, HeaderDefinition::SUB_PAYLOAD_INDEX + 1 + ExtraSize>
	{
		static constexpr uint8_t PAYLOAD_REQUEST_ID_INDEX = HeaderDefinition::SUB_PAYLOAD_INDEX;
	};

	/// <summary>
	/// Dynamic size packet.
	/// SubPayload: ||RequestId|Data...||
	/// </summary>
	struct DeliveryRequestDefinition : public DeliveryIdDefinition<0, MAX_DATA_SIZE>
	{
		static constexpr uint8_t PAYLOAD_DATA_INDEX = DeliveryIdDefinition<0, MAX_DATA_SIZE>::PAYLOAD_REQUEST_ID_INDEX + 1;
	};


	/// <summary>
	/// 
	/// SubPayload: ||RequestId||
	/// </summary>
	using DeliveryReplyDefinition = DeliveryIdDefinition<DeliveryRequestDefinition::HEADER + 1>;

	/// <summary>
	/// 
	/// SubPayload: ||RequestId||
	/// </summary>
	using DeliveryAckDefinition = DeliveryIdDefinition<DeliveryReplyDefinition::HEADER + 1>;

	static constexpr uint8_t DELIVERY_SUB_SERVICE_HEADER_START = DeliveryAckDefinition::HEADER + 1;
};
#endif