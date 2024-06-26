// LoLaPacketDefinition.h

#ifndef _LOLA_PACKET_DEFINITION_h
#define _LOLA_PACKET_DEFINITION_h

#include <stdint.h>

/// <summary>
/// LoLa Packet Frame
/// ________	Frame Start.
/// MAC_0			MAC 4 Bytes.
/// MAC_1
/// MAC_2
/// MAC_3
/// ________	Content start.
/// ID_0			Id rolling counter 2 Bytes.
/// ID_1
/// ________	Data Start.
/// PORT			Endpoint port 1 Byte.
/// 
/// ________	Payload Start.
/// PAYLOAD_0		Payload N Bytes.
/// PAYLOAD_1
/// PAYLOAD_N
/// ________
/// </summary>
namespace LoLaPacketDefinition
{
	static constexpr uint8_t MAC_INDEX = 0;
	static constexpr uint8_t MAC_SIZE = 4;
	static constexpr uint8_t ID_INDEX = (MAC_INDEX + MAC_SIZE);
	static constexpr uint8_t ID_SIZE = 2;
	static constexpr uint8_t PORT_INDEX = ID_INDEX + ID_SIZE;
	static constexpr uint8_t PORT_SIZE = 1;
	static constexpr uint8_t PAYLOAD_INDEX = PORT_INDEX + PORT_SIZE;

	static constexpr uint8_t CONTENT_INDEX = ID_INDEX;
	static constexpr uint8_t CONTENT_SIZE_SIZE = 1;

	static constexpr uint8_t DATA_INDEX = PORT_INDEX;

	static constexpr uint8_t MIN_PACKET_SIZE = PAYLOAD_INDEX;

	/// <summary>
	/// Packet is limited to reduce buffer sizes,
	/// and fixed to 32 which is the security limit of our stream key.
	/// </summary>
	static constexpr uint8_t MAX_PACKET_TOTAL_SIZE = 32;


	static constexpr uint8_t MAX_PAYLOAD_SIZE = MAX_PACKET_TOTAL_SIZE - PAYLOAD_INDEX;

	static constexpr uint8_t GetContentSizeFromDataSize(const uint8_t dataSize)
	{
		return dataSize + (DATA_INDEX - CONTENT_INDEX);
	}

	static constexpr uint8_t GetDataSize(const uint8_t totalSize)
	{
		return totalSize - DATA_INDEX;
	}

	static constexpr uint8_t GetDataSizeFromPayloadSize(const uint8_t payloadSize)
	{
		return payloadSize + (PAYLOAD_INDEX - DATA_INDEX);
	}

	static constexpr uint8_t GetPayloadSize(const uint8_t totalSize)
	{
		return totalSize - PAYLOAD_INDEX;
	}

	static constexpr uint8_t GetTotalSize(const uint8_t payloadSize)
	{
		return PAYLOAD_INDEX + payloadSize;
	}
};

/// <summary>
/// Template packet definition.
/// </summary>
template<const uint8_t Port, const uint8_t PayloadSize>
struct TemplateDefinition
{
	static constexpr uint8_t PORT = Port;
	static constexpr uint8_t PAYLOAD_SIZE = PayloadSize;

	static constexpr uint8_t PACKET_SIZE = LoLaPacketDefinition::GetTotalSize(PAYLOAD_SIZE);
	static constexpr uint8_t DATA_SIZE = LoLaPacketDefinition::GetDataSize(PACKET_SIZE);
};

struct HeaderDefinition
{
	static constexpr uint8_t HEADER_INDEX = 0;
	static constexpr uint8_t SUB_PAYLOAD_INDEX = 1;
};

/// <summary>
/// Wraps Payload as |Header|SubPayload...|
/// </summary>
/// <typeparam name="Header"></typeparam>
/// <typeparam name="SubPayloadSize">[0 ; LoLaPacketDefinition::MAX_PAYLOAD_SIZE - 1]</typeparam>
template<const uint8_t Header,
	const uint8_t SubPayloadSize>
struct TemplateHeaderDefinition : public HeaderDefinition
{
	static constexpr uint8_t HEADER = Header;

	static constexpr uint8_t SUB_PAYLOAD_SIZE = SubPayloadSize;

	static constexpr uint8_t PAYLOAD_SIZE = SUB_PAYLOAD_SIZE + 1;

	static constexpr uint8_t PACKET_SIZE = LoLaPacketDefinition::GetTotalSize(PAYLOAD_SIZE);
};

template<const uint8_t MaxPayloadSize>
class TemplateLoLaOutDataPacket
{
private:
	static constexpr uint8_t DataSize = LoLaPacketDefinition::GetDataSizeFromPayloadSize(MaxPayloadSize);

public:
	uint8_t Data[DataSize];

	/// <summary>
	/// Simplified writing to payload.
	/// </summary>
	uint8_t* Payload;

public:
	TemplateLoLaOutDataPacket(const uint8_t port = 0)
		: Payload(&Data[LoLaPacketDefinition::PAYLOAD_INDEX - LoLaPacketDefinition::DATA_INDEX])
	{
		SetPort(port);
	}

	const uint8_t GetPort()
	{
		return Data[LoLaPacketDefinition::PORT_INDEX - LoLaPacketDefinition::DATA_INDEX];
	}

	void SetPort(const uint8_t port)
	{
		Data[LoLaPacketDefinition::PORT_INDEX - LoLaPacketDefinition::DATA_INDEX] = port;
	}

	const uint8_t GetHeader()
	{
		return Data[LoLaPacketDefinition::PAYLOAD_INDEX + HeaderDefinition::HEADER_INDEX - LoLaPacketDefinition::DATA_INDEX];
	}

	void SetHeader(const uint8_t header)
	{
		Data[LoLaPacketDefinition::PAYLOAD_INDEX + HeaderDefinition::HEADER_INDEX - LoLaPacketDefinition::DATA_INDEX] = header;
	}
};
#endif