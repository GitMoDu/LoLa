// AbstractPublicKeyLoLaLink.h

#ifndef _ABSTRACT_PUBLIC_KEY_LOLA_LINK_
#define _ABSTRACT_PUBLIC_KEY_LOLA_LINK_

#include "AbstractLoLaLink.h"

#include "..\..\Crypto\LoLaCryptoPkeSession.h"

/// <summary>
/// Elliptic-curve Diffie–Hellman public key exchange.
/// SECP 160 R1
/// Depends on https://github.com/kmackay/micro-ecc .
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractPublicKeyLoLaLink : public AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;

protected:
	using BaseClass::RandomSource;

	using BaseClass::ExpandedKey;

protected:
	LoLaCryptoPkeSession Session;

public:
	AbstractPublicKeyLoLaLink(Scheduler& scheduler,
		ILoLaTransceiver* transceiver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop,
		const uint8_t* publicKey,
		const uint8_t* privateKey,
		const uint8_t* accessPassword)
		: BaseClass(scheduler, &Session, transceiver, entropySource, clockSource, timerSource, duplex, hop)
		, Session(&ExpandedKey, accessPassword, publicKey, privateKey)
	{}

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
			Session.GenerateLocalChallenge(&RandomSource);
			break;
		default:
			break;
		}
	}
};
#endif