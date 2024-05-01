// BinaryDeliveryDefinitions.h

#ifndef _BINARY_DELIVERY_DEFINITIONS_h
#define _BINARY_DELIVERY_DEFINITIONS_h

#include "..\..\LoLaLink\LoLaPacketDefinition.h"
#include <IBinaryDelivery.h>


/// <summary>
/// Sender requests start of send to receiver.
/// SubPayload: ||DataSize[0..3]||
/// </summary>
class SenderStartSubDefinition : public TemplateSubHeaderDefinition<0, sizeof(uint32_t)>
{};

/// <summary>
/// Sender requests validation of data CRC with receiver.
/// SubPayload: ||CRC[0..3]||
/// </summary>
class SenderEndSubDefinition : public TemplateSubHeaderDefinition<SenderStartSubDefinition::SUB_HEADER + 1, sizeof(uint32_t)>
{};

/// <summary>
/// Sender has data for the receiver.
/// SubPayload: ||Address[0..3]|Size|Data[0..IBinaryDelivery::MaxChunkSize]||
/// </summary>
class SenderSendDataSubDefinition :
	public TemplateSubHeaderDefinition<SenderEndSubDefinition::SUB_HEADER + 1,
	sizeof(uint32_t) + sizeof(uint8_t) + IBinaryDelivery::MaxChunkSize>
{};

static const uint8_t BinaryDeliveryMaxPayloadSize = SenderSendDataSubDefinition::PAYLOAD_SIZE;

#endif