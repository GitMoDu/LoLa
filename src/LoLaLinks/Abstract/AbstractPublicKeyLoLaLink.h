// AbstractPublicKeyLoLaLink.h

#ifndef _ABSTRACT_PUBLIC_KEY_LOLA_LINK_
#define _ABSTRACT_PUBLIC_KEY_LOLA_LINK_

// https://github.com/kmackay/micro-ecc .
#include <uECC.h>

#include "AbstractLoLaReceiver.h"

/// <summary>
/// Elliptic-curve Diffie�Hellman public key exchange.
/// SECP 160 R1
/// Depends on https://github.com/kmackay/micro-ecc .
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractPublicKeyLoLaLink : public AbstractLoLaReceiver<LoLaLinkDefinition::LARGEST_PAYLOAD, MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaReceiver<LoLaLinkDefinition::LARGEST_PAYLOAD, MaxPacketReceiveListeners, MaxLinkListeners>;

protected:
	using BaseClass::Session;

	using BaseClass::RawOutPacket;
	using BaseClass::RawInPacket;
	using BaseClass::InData;

public:
	AbstractPublicKeyLoLaLink(Scheduler& scheduler,
		ILoLaPacketDriver* driver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop,
		const uint8_t* publicKey,
		const uint8_t* privateKey,
		const uint8_t* accessPassword)
		: BaseClass(scheduler, driver, entropySource, clockSource, timerSource, duplex, hop)
	{
		Session.SetKeysAndPassword(publicKey, privateKey, accessPassword);
	}

	virtual const bool Setup()
	{
		return Session.Setup() && BaseClass::Setup();
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Linking:
			Session.GenerateLocalChallenge();
			break;
		default:
			break;
		}
	}

protected:
	virtual void EncodeOutPacket(const uint8_t* data, const uint8_t counter, const uint8_t dataSize) final
	{
		Session.EncodeOutPacket(data, RawOutPacket, counter, dataSize);
	}

	virtual void EncodeOutPacket(const uint8_t* data, Timestamp& timestamp, const uint8_t counter, const uint8_t dataSize) final
	{
		Session.EncodeOutPacket(data, RawOutPacket, timestamp, counter, dataSize);
	}

	virtual const bool DecodeInPacket(uint8_t& counter, const uint8_t dataSize)  final
	{
		return Session.DecodeInPacket(RawInPacket, InData, counter, dataSize);
	}

	virtual const bool DecodeInPacket(Timestamp& timestamp, uint8_t& counter, const uint8_t dataSize)  final
	{
		return Session.DecodeInPacket(RawInPacket, InData, timestamp, counter, dataSize);
	}
};
#endif