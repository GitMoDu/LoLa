//// LoLaPacket.h
//
//#ifndef _LOLAPACKET_h
//#define _LOLAPACKET_h
//
//#include <Arduino.h>
//#include "PacketDefinition.h"
//
//class LoLaPacket
//{
//private:
//
//
//protected:
//	uint8_t* Raw = nullptr;
//
//public:
//	virtual uint8_t GetMaxSize() { return 0; }
//
//public:
//	LoLaPacket(uint8_t* raw) : Raw(raw)
//	{
//	}
//
//	uint8_t* GetRaw()
//	{
//		return Raw;
//	}
//
//	void SetHeader(const uint8_t header)
//	{
//		Raw[PacketDefinition::LOLA_PACKET_HEADER_INDEX] = header;
//	}
//
//	const uint32_t GetMACCRC()
//	{
//		ArrayToUint32 ATUI;
//		ATUI.array[0] = Raw[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 0];
//		ATUI.array[1] = Raw[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 1];
//		ATUI.array[2] = Raw[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 2];
//		ATUI.array[3] = Raw[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 3];
//
//		return ATUI.uint;
//	}
//
//	void SetMACCRC(const uint32_t crcValue)
//	{
//		ArrayToUint32 ATUI;
//
//		ATUI.uint = crcValue;
//		Raw[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 0] = ATUI.array[0];
//		Raw[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 1] = ATUI.array[1];
//		Raw[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 2] = ATUI.array[2];
//		Raw[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 3] = ATUI.array[3];
//	}
//
//	const uint8_t GetHeader()
//	{
//		return Raw[PacketDefinition::LOLA_PACKET_HEADER_INDEX];
//	}
//
//	// Special getter for Id, can only be read by services.
//	const uint8_t GetId()
//	{
//		return Raw[PacketDefinition::LOLA_PACKET_ID_INDEX];
//	}
//
//	// Everything but the MAC/CRC.
//	uint8_t* GetContent()
//	{
//		return &Raw[PacketDefinition::LOLA_PACKET_ID_INDEX];
//	}
//
//	uint8_t* GetPayload()
//	{
//		return &Raw[PacketDefinition::LOLA_PACKET_PAYLOAD_INDEX];
//	}
//};
//
//template <const uint8_t PayloadSize>
//class TemplateLoLaPacket : public LoLaPacket
//{
//private:
//	static const uint8_t TotalSize = PacketDefinition::GetTotalSize(PayloadSize);
//
//	uint8_t Data[TotalSize];
//
//public:
//	TemplateLoLaPacket() : LoLaPacket(Data)
//	{
//	}
//
//	const uint8_t GetTotalSize()
//	{
//		return TotalSize;
//	}
//};
//
//class NotifiableIncomingPacket : public TemplateLoLaPacket<PacketDefinition::LOLA_PACKET_MAX_PAYLOAD_SIZE>
//{
//public:
//	NotifiableIncomingPacket()
//		: TemplateLoLaPacket<PacketDefinition::LOLA_PACKET_MAX_PAYLOAD_SIZE>()
//	{}
//
//	bool NotifyPacketReceived(PacketDefinition* definition, const uint32_t timestamp)
//	{
//		return definition->Service->OnPacketReceived(GetHeader(), timestamp, GetPayload());
//	}
//};
//
//class OutgoingIdPacket : public TemplateLoLaPacket<PacketDefinition::LOLA_PACKET_MAX_PAYLOAD_SIZE>
//{
//public:
//	OutgoingIdPacket() : TemplateLoLaPacket<PacketDefinition::LOLA_PACKET_MAX_PAYLOAD_SIZE>() {}
//
//	// Special setter for Id, can only be writen by the driver.
//	void SetId(const uint8_t id)
//	{
//		Raw[PacketDefinition::LOLA_PACKET_ID_INDEX] = id;
//	}
//};
//#endif