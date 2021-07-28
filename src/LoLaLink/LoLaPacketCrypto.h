// LoLaPacketCrypto.h

#ifndef _LOLA_PACKET_CRYPTO_h
#define _LOLA_PACKET_CRYPTO_h


#include <LoLaLink\LoLaPacketService.h>


template<const uint8_t MaxPacketSize>
class LoLaPacketCrypto : protected LoLaPacketService<MaxPacketSize>
{
protected:
	using LoLaPacketService<MaxPacketSize>::InMessage;
	using LoLaPacketService<MaxPacketSize>::OutMessage;

public:
	// Runtime Flags.
	bool TokenHopEnabled = false;
	bool EncryptionEnabled = false;

	LoLaCryptoEncoder CryptoEncoder;

	/*TOTPSecondsTokenGenerator LinkTokenGenerator;
	TOTPMillisecondsTokenGenerator ChannelTokenGenerator;*/

public:
	LoLaPacketCrypto(Scheduler* scheduler, ILoLaPacketDriver* driver)
		: LoLaPacketService<MaxPacketSize>(scheduler, driver)
		, CryptoEncoder()
		//, LinkTokenGenerator(syncedClock)
		//, ChannelTokenGenerator(syncedClock, LOLA_LINK_SERVICE_LINKED_CHANNEL_HOP_PERIOD_MILLIS)
	{
	}

	virtual const bool Setup()
	{
		if (LoLaPacketService<MaxPacketSize>::Setup())
		{
			CryptoEncoder.Setup();
			return true;
		}
		else
		{
			return false;
		}
	}

protected:
	void EncodeOutputContent(const uint32_t token, const uint8_t contentSize)
	{
		uint32_t mac;
		if (EncryptionEnabled)
		{
			mac = CryptoEncoder.Encode(
				token,
				&OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX],
				contentSize);
		}
		else
		{
			mac = CryptoEncoder.NoEncode(
				token,
				&OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX],
				contentSize);
		}

		// Set MAC/CRC.
		SetOutMac(mac);
	}

	const bool DecodeInputContent(const uint32_t token, const uint8_t contentSize)
	{
		uint32_t macCrc = InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 0];
		macCrc += InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 1] << 8;
		macCrc += InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 2] << 16;
		macCrc += InMessage[PacketDefinition::LOLA_PACKET_MACCRC_INDEX + 3] << 24;

		if (EncryptionEnabled)
		{
			return CryptoEncoder.Decode(
				macCrc,
				token,
				&OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX],
				contentSize);
		}
		else
		{
			return CryptoEncoder.NoDecode(
				macCrc,
				token,
				&OutMessage[PacketDefinition::LOLA_PACKET_ID_INDEX],
				contentSize);
		}
	}
};
#endif