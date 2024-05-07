// LolaAddressMatchLinkClient.h

#ifndef _LOLA_ADDRESS_MATCH_LINK_CLIENT_
#define _LOLA_ADDRESS_MATCH_LINK_CLIENT_

#include "../Abstract/AbstractLoLaLinkClient.h"
#include "../../Crypto/LoLaCryptoAmSession.h"

/// <summary>
/// LoLa Address Match Link Client.
/// Template parameters allow specifying maxlisteners.
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaAddressMatchLinkClient : public AbstractLoLaLinkClient
{
private:
	using BaseClass = AbstractLoLaLinkClient;

private:
	LinkRegistry<MaxPacketReceiveListeners, MaxLinkListeners> RegistryInstance{};

	LoLaCryptoAmSession Session;

public:
	LoLaAddressMatchLinkClient(Scheduler& scheduler,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop,
		const uint8_t* secretKey,
		const uint8_t* accessPassword,
		const uint8_t* localAddress)
		: BaseClass(scheduler, &RegistryInstance, &Session, transceiver, cycles, entropy, duplex, hop)
		, Session(secretKey, accessPassword, localAddress)
	{}

	const bool Setup()
	{
		if (Session.Setup())
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
			if (UnlinkedPacketThrottle())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Unlinked::AmLinkingStartRequest::HEADER);
				Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::AmLinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]);
				Session.CopyLocalAddressTo(&OutPacket.Payload[Unlinked::AmLinkingStartRequest::PAYLOAD_CLIENT_ADDRESS_INDEX]);
				Session.CopyPartnerAddressTo(&OutPacket.Payload[Unlinked::AmLinkingStartRequest::PAYLOAD_SERVER_ADDRESS_INDEX]);

				LOLA_RTOS_PAUSE();
				if (UnlinkedDuplexCanSend(Unlinked::AmLinkingStartRequest::PAYLOAD_SIZE) &&
					PacketService.CanSendPacket())
				{
					if (SendPacket(OutPacket.Data, Unlinked::AmLinkingStartRequest::PAYLOAD_SIZE))
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Sent AmLinkingStartRequest"));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
		}
		else if (Session.HasPartner())
		{
			Session.Calculate();
		}
		else if (UnlinkedPacketThrottle())
		{
			LOLA_RTOS_PAUSE();
			if (UnlinkedDuplexCanSend(Unlinked::AmSessionRequest::PAYLOAD_SIZE)
				&& PacketService.CanSendPacket())
			{
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.SetHeader(Unlinked::AmSessionRequest::HEADER);
				if (SendPacket(OutPacket.Data, Unlinked::AmSessionRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Sent AmSessionRequest"));
#endif
				}
			}
			LOLA_RTOS_RESUME();
		}
		Task::enableDelayed(0);
	}

	void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint16_t rollingCounter, const uint8_t payloadSize) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::AmSessionAvailable::HEADER:
			if (payloadSize == Unlinked::AmSessionAvailable::PAYLOAD_SIZE)
			{
				switch (LinkStage)
				{
				case LinkStageEnum::Pairing:
					Session.ResetAm();
					Session.SetSessionId(&payload[Unlinked::AmSessionAvailable::PAYLOAD_SESSION_ID_INDEX]);
					Session.SetPartnerAddressFrom(&payload[Unlinked::AmSessionAvailable::PAYLOAD_SERVER_ADDRESS_INDEX]);
					if (!Session.AddressCollision())
					{
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Found an Address Match Session."));
#endif
						OnLinkSyncReceived(timestamp);
						Task::enableDelayed(0);
					}
#if defined(DEBUG_LOLA_LINK)
					else { this->Skipped(F("AmSessionAvailable2")); }
#endif
					break;
				default:
					return;
					break;
				}
			}
#if defined(DEBUG_LOLA_LINK)
			else {
				this->Skipped(F("AmSessionAvailable"));
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