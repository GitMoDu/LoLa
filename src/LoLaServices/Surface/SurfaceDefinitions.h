// SyncSurfaceDefinitions.h

#ifndef _SYNC_SURFACE_DEFINITIONS_h
#define _SYNC_SURFACE_DEFINITIONS_h

#include "ISurface.h"

namespace SurfaceDefinitions
{
	/// <summary>
	/// Fletcher 16.
	/// </summary>
	static constexpr uint8_t CRC_SIZE = 2;

	/// <summary>
	/// Requests to invalidate all blocks on the writer side.
	/// SubPayload: ||CRC0|CRC1||
	/// </summary>
	template<const uint8_t SubHeader>
	struct AbstractSyncMetaDefinition : public TemplateHeaderDefinition<SubHeader, CRC_SIZE>
	{
		static constexpr uint8_t CRC_OFFSET = HeaderDefinition::SUB_PAYLOAD_INDEX;
	};

	using SyncMetaDefinition = AbstractSyncMetaDefinition<0>;

	/// <summary>
	/// Request writer to invalidate all blocks.
	/// SubPayload: ||CRC0|CRC1||
	/// </summary>
	using ReaderInvalidateDefinition = AbstractSyncMetaDefinition<0>;

	/// <summary>
	/// Inform writer of the sync status..
	/// SubPayload: ||CRC0|CRC1||
	/// </summary>
	using ReaderValidateDefinition = AbstractSyncMetaDefinition<ReaderInvalidateDefinition::HEADER + 1>;

	/// <summary>
	/// Request the reader to reply with the sync status.
	/// SubPayload: ||CRC0|CRC1||
	/// </summary>
	using WriterCheckHashDefinition = AbstractSyncMetaDefinition<ReaderValidateDefinition::HEADER + 1>;

	/// <summary>
	/// Send one block of data to update on the reader side.
	/// SubPayload: ||Index|B0..BytesPerBlock||
	/// </summary>
	struct WriterUpdateBlock1xDefinition : public TemplateHeaderDefinition<
		WriterCheckHashDefinition::HEADER + 1
		, 1 + ISurface::BytesPerBlock>
	{
		static constexpr uint8_t INDEX_OFFSET = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t BLOCKS_OFFSET = INDEX_OFFSET + sizeof(uint8_t);
	};

	/// <summary>
	/// Send two blocks of data to update on the reader side.
	/// SubPayload: ||Index|B0..BytesPerBlock|B1..BytesPerBlock||
	/// </summary>
	struct WriterUpdateBlock2xDefinition : public TemplateHeaderDefinition<
		WriterUpdateBlock1xDefinition::HEADER + 1
		, 1 + (2 * ISurface::BytesPerBlock)>
	{
		static constexpr uint8_t INDEX_OFFSET = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t BLOCK1_OFFSET = INDEX_OFFSET + sizeof(uint8_t);
		static constexpr uint8_t BLOCK2_OFFSET = BLOCK1_OFFSET + ISurface::BytesPerBlock;
	};

	static constexpr uint8_t SyncSurfaceMaxPayloadSize = WriterUpdateBlock2xDefinition::PAYLOAD_SIZE;
	static constexpr uint8_t SyncSurfaceMetaPayloadSize = ReaderInvalidateDefinition::PAYLOAD_SIZE;
}
#endif