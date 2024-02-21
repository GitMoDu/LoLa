// SurfaceDefinitions.h

#ifndef _SURFACE_DEFINITIONS_h
#define _SURFACE_DEFINITIONS_h

#include "ISurface.h"
#include "../../Link/LoLaPacketDefinition.h"
#include <stddef.h>

namespace SurfaceDefinitions
{
	/// <summary>
	/// Fletcher 16.
	/// </summary>
	static constexpr uint8_t CRC_SIZE = 2;

	/// <summary>
	/// Template request to (in)validate block data on the Writer side.
	/// SubPayload: ||CRC0|CRC1||
	/// </summary>
	template<const uint8_t Header>
	struct SyncDefinition : public TemplateHeaderDefinition<Header, CRC_SIZE>
	{
		static constexpr uint8_t CRC_OFFSET = HeaderDefinition::SUB_PAYLOAD_INDEX;
	};

	/// <summary>
	/// Template request to update multiple blocks of data on the Reader side.
	/// Header range includes all headers from HEADER to MAX_HEADER.
	/// Index can be extracted from header.
	/// SubPayload: ||B0..BytesPerBlock|B1..BytesPerBlock|B2..BytesPerBlock||
	/// </summary>
	template<const uint8_t Header,
		const uint8_t BlockCount>
	struct BlockRangeDefinition : public TemplateHeaderDefinition<Header, BlockCount* ISurface::BytesPerBlock>
	{
		static constexpr uint8_t BLOCK0_OFFSET = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t BLOCK1_OFFSET = BLOCK0_OFFSET + ISurface::BytesPerBlock;
		static constexpr uint8_t BLOCK2_OFFSET = BLOCK1_OFFSET + ISurface::BytesPerBlock;
		static constexpr uint8_t HEADER_RANGE = UINT8_MAX / BlockCount;
		static constexpr uint8_t MAX_HEADER = Header + HEADER_RANGE;

		static constexpr uint8_t GetEmbeddedValueFromHeader(const uint8_t header)
		{
			return (header - Header) * BlockCount;
		}

		static constexpr uint8_t GetHeaderFromEmbeddedValue(const uint8_t value)
		{
			return Header + (value / BlockCount);
		}
	};

	/// <summary>
	/// Request writer to invalidate all blocks.
	/// SubPayload: ||CRC0|CRC1||
	/// </summary>
	using ReaderInvalidateDefinition = SyncDefinition<0>;

	/// <summary>
	/// Inform writer of the sync status..
	/// SubPayload: ||CRC0|CRC1||
	/// </summary>
	using ReaderValidateDefinition = SyncDefinition<ReaderInvalidateDefinition::HEADER + 1>;

	/// <summary>
	/// Request the reader to reply with the sync status.
	/// SubPayload: ||CRC0|CRC1||
	/// </summary>
	using WriterCheckHashDefinition = SyncDefinition<ReaderValidateDefinition::HEADER + 1>;

	/// <summary>
	/// Send 1 block of data to update on the reader side.
	/// SubPayload: ||Index|B0..BytesPerBlock||
	/// </summary>
	struct WriterDefinition1x1 : public TemplateHeaderDefinition<
		WriterCheckHashDefinition::HEADER + 1
		, 1 + ISurface::BytesPerBlock>
	{
		static constexpr uint8_t INDEX_OFFSET = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t BLOCK_OFFSET = INDEX_OFFSET + sizeof(uint8_t);
	};

	/// <summary>
	/// Send 2 indexed blocks of data to update on the reader side.
	/// SubPayload: ||Index0|B0..BytesPerBlock|Index1|B1..BytesPerBlock||
	/// </summary>
	struct WriterDefinition2x1 : public TemplateHeaderDefinition<
		WriterDefinition1x1::HEADER + 1
		, 2 * (1 + ISurface::BytesPerBlock)>
	{
		static constexpr uint8_t INDEX0_OFFSET = HeaderDefinition::SUB_PAYLOAD_INDEX;
		static constexpr uint8_t INDEX1_OFFSET = INDEX0_OFFSET + sizeof(uint8_t);
		static constexpr uint8_t BLOCK0_OFFSET = INDEX1_OFFSET + sizeof(uint8_t);
		static constexpr uint8_t BLOCK1_OFFSET = BLOCK0_OFFSET + ISurface::BytesPerBlock;
	};

	/// <summary>
	/// Send 2 sequential blocks of data to update on the reader side.
	/// Index is encoded in header.
	/// SubPayload: ||B0..BytesPerBlock|B1..BytesPerBlock||
	/// </summary>
	using WriterDefinition1x2 = BlockRangeDefinition<WriterDefinition2x1::HEADER + 1, 2>;

	/// <summary>
	/// Send 3 sequential blocks of data to update on the reader side.
	/// Index is encoded in header.
	/// SubPayload: ||B0..BytesPerBlock|B1..BytesPerBlock|B2..BytesPerBlock||
	/// </summary>
	using WriterDefinition1x3 = BlockRangeDefinition<WriterDefinition1x2::MAX_HEADER + 1, 3>;

	static constexpr uint8_t SurfaceWriterMaxPayloadSize = WriterDefinition1x3::PAYLOAD_SIZE;
	static constexpr uint8_t SurfaceReaderMaxPayloadSize = WriterCheckHashDefinition::PAYLOAD_SIZE;
}
#endif