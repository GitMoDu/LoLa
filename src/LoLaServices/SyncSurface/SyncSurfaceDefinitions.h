// SyncSurfaceDefinitions.h

#ifndef _SYNC_SURFACE_DEFINITIONS_h
#define _SYNC_SURFACE_DEFINITIONS_h

#include <ITrackedSurface.h>


/// <summary>
/// Requests to invalidate all blocks on the writer side.
/// SubPayload: ||CRC||
/// </summary>
template<const uint8_t SubHeader>
class SyncMetaSubDefinition : public TemplateSubHeaderDefinition<SubHeader, 1>
{
public:
	static const uint8_t CRC_OFFSET = SubHeaderDefinition::SUB_HEADER_INDEX;
};

using SyncCommonSubDefinition = SyncMetaSubDefinition<0>;

//static const uint8_t SyncMetaPacketSize = TemplateSubDefinition<0, 1>::PACKET_SIZE;

/// <summary>
/// Requests to invalidate all blocks on the writer side.
/// SubPayload: ||CRC||
/// </summary>
using ReaderInvalidateSubDefinition = SyncMetaSubDefinition<0>;

/// <summary>
/// Informs writer that the CRC has matched.
/// SubPayload: ||CRC||
/// </summary>
using ReaderValidateHashSubDefinition = SyncMetaSubDefinition<ReaderInvalidateSubDefinition::SUB_HEADER + 1>;

/// <summary>
/// Sends one block of data to update on the reader side.
/// SubPayload: ||CRC|Index|B0..BytesPerBlock||
/// </summary>
struct  WriterUpdateBlockSubDefinition : public TemplateSubHeaderDefinition<ReaderValidateHashSubDefinition::SUB_HEADER + 1, 1 + 1 + 8>
{
	static const uint8_t CRC_OFFSET = SubHeaderDefinition::SUB_HEADER_INDEX;
	static const uint8_t INDEX_OFFSET = CRC_OFFSET + 1;
	static const uint8_t BLOCKS_OFFSET = INDEX_OFFSET + 1;
};

/// <summary>
/// Requests to check for CRC match on the reader side.
/// SubPayload: ||CRC||
/// </summary>
class WriterCheckHashSubDefinition : public SyncMetaSubDefinition<WriterUpdateBlockSubDefinition::SUB_HEADER + 1> {};

static const uint8_t SyncSurfaceMaxPayloadSize = WriterUpdateBlockSubDefinition::PAYLOAD_SIZE;
static const uint8_t SyncSurfaceMetaPayloadSize = ReaderInvalidateSubDefinition::PAYLOAD_SIZE;

#endif