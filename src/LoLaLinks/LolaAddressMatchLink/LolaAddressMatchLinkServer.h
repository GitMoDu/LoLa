// LoLaAddressMatchLinkServer.h

#ifndef _LOLA_ADDRESS_MATCH_LINK_SERVER_
#define _LOLA_ADDRESS_MATCH_LINK_SERVER_

#include "../Abstract/AbstractLoLaLinkServer.h"
#include "../../Crypto/LoLaCryptoAmSession.h"

/// <summary>
/// LoLa Public-Key-Exchange Link Server.
/// Template parameters allow specifying maxlisteners.
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaAddressMatchLinkServer : public AbstractLoLaLinkServer
{
private:
	using BaseClass = AbstractLoLaLinkServer;

private:
	LinkRegistry<MaxPacketReceiveListeners, MaxLinkListeners> RegistryInstance{};

	bool AmReplyPending = false;

public:
	LoLaAddressMatchLinkServer(TS::Scheduler& scheduler,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, &RegistryInstance, transceiver, cycles, entropy, duplex, hop)
	{}

	const bool Setup(const uint8_t localAddress[LoLaLinkDefinition::PUBLIC_ADDRESS_SIZE],
		const uint8_t accessPassword[LoLaLinkDefinition::ACCESS_CONTROL_PASSWORD_SIZE],
		const uint8_t secretKey[LoLaLinkDefinition::SECRET_KEY_SIZE])
	{
		if (Session.Setup()
			&& Session.SetKeys(localAddress, accessPassword, secretKey))
		{
			return RegistryInstance.Setup() && BaseClass::Setup();
		}
#if defined(DEBUG_LOLA)
		else
		{
			this->Owner();
			Serial.println(F("Address Match Session setup failed."));
		}
#endif
		return false;
	}

protected:
	void OnServicePairing() final
	{
		if (Session.Ready())
		{
			UpdateLinkStage(LinkStageEnum::SwitchingToLinking);
		}
		else if (Session.HasPartner())
		{
			Session.Calculate();
		}
		else if (AmReplyPending)
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.SetHeader(Unlinked::AmSessionAvailable::HEADER);
			Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::AmSessionAvailable::PAYLOAD_SESSION_ID_INDEX]);
			Session.CopyLocalAddressTo(&OutPacket.Payload[Unlinked::AmSessionAvailable::PAYLOAD_SERVER_ADDRESS_INDEX]);

			LOLA_RTOS_PAUSE();
			if (UnlinkedDuplexCanSend(Unlinked::AmSessionAvailable::PAYLOAD_SIZE) &&
				PacketService.CanSendPacket())
			{
				AmReplyPending = false;
				if (SendPacket(OutPacket.Data, Unlinked::AmSessionAvailable::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Sent AmSessionAvailable"));
#endif
				}
			}
			LOLA_RTOS_RESUME();
		}

		TS::Task::enableDelayed(0);
	}

	void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint16_t rollingCounter, const uint8_t payloadSize) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::AmSessionRequest::HEADER:
			if (payloadSize == Unlinked::AmSessionRequest::PAYLOAD_SIZE
				&& !Session.HasPartner())
			{
				switch (LinkStage)
				{
				case LinkStageEnum::Searching:
					Session.ResetAm();
					UpdateLinkStage(LinkStageEnum::Pairing);
				case LinkStageEnum::Pairing:
					AmReplyPending = true;
					TS::Task::enableDelayed(0);
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("AmSessionRequest received."));
#endif
					break;
				default:
#if defined(DEBUG_LOLA_LINK)
					this->Skipped(F("AmSessionRequest2"));
#endif
					return;
					break;
				}
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("AmSessionRequest")); }
#endif
			break;
		case Unlinked::AmLinkingStartRequest::HEADER:
			if (payloadSize == Unlinked::AmLinkingStartRequest::PAYLOAD_SIZE
				&& LinkStage == LinkStageEnum::Pairing
				&& !Session.HasPartner()
				&& Session.LocalAddressMatchFrom(&payload[Unlinked::AmLinkingStartRequest::PAYLOAD_SERVER_ADDRESS_INDEX])
				&& Session.SessionIdMatches(&payload[Unlinked::AmLinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]))
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("AmLinkingStartRequest received"));
#endif				
				Session.SetPartnerAddressFrom(&payload[Unlinked::AmLinkingStartRequest::PAYLOAD_CLIENT_ADDRESS_INDEX]);
				TS::Task::enableDelayed(0);
			}
#if defined(DEBUG_LOLA_LINK)
			else {
				this->Skipped(F("AmLinkingStartRequest"));
			}
#endif
			break;
		default:
			BaseClass::OnUnlinkedPacketReceived(timestamp, payload, rollingCounter, payloadSize);
			break;
		}
	}

	void UpdateLinkStage(const LinkStageEnum linkStage) final
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Searching:
		case LinkStageEnum::Pairing:
			Session.ResetAm();
			break;
		default:
			break;
		}
	}
};
#endif