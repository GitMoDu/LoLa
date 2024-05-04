// LolaAddressMatchLinkClient.h

#ifndef _LOLA_ADDRESS_MATCH_LINK_CLIENT_
#define _LOLA_ADDRESS_MATCH_LINK_CLIENT_

#include "..\Abstract\AbstractLoLaLinkClient.h"
#include "..\..\Crypto\LoLaCryptoAmSession.h"

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
	enum class AmStateEnum : uint8_t
	{
		RequestingSession,
		ValidatingSession,
		ComputingSecretKey,
		SessionCached
	};

private:
	LinkRegistry<MaxPacketReceiveListeners, MaxLinkListeners> RegistryInstance{};

	LoLaCryptoAmSession Session;

	AmStateEnum AmState = AmStateEnum::RequestingSession;

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
	void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint16_t rollingCounter, const uint8_t payloadSize) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::AmSessionAvailable::HEADER:
			if (payloadSize == Unlinked::AmSessionAvailable::PAYLOAD_SIZE
				&& IsInSessionCreation())
			{
				switch (AmState)
				{
				case AmStateEnum::RequestingSession:;
					AmState = AmStateEnum::ValidatingSession;
					Session.SetSessionId(&payload[Unlinked::AmSessionAvailable::PAYLOAD_SESSION_ID_INDEX]);
					Session.SetPartnerAddressFrom(&payload[Unlinked::AmSessionAvailable::PAYLOAD_SERVER_ADDRESS_INDEX]);
					OnLinkSyncReceived(timestamp);
					ResetUnlinkedPacketThrottle();
					Task::enable();
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Found an Address Match Session."));
#endif
					break;
				default:
#if defined(DEBUG_LOLA_LINK)
					this->Skipped(F("SessionAvailable"));
#endif
					break;
				}
			}
#if defined(DEBUG_LOLA_LINK)
			else {
				this->Skipped(F("SessionAvailable"));
			}
#endif
			break;
		default:
			BaseClass::OnUnlinkedPacketReceived(timestamp, payload, rollingCounter, payloadSize);
			break;
		}
	}

protected:
	void ResetSessionCreation() final
	{
		AmState = AmStateEnum::RequestingSession;
		Session.ResetAm();
		ResetUnlinkedPacketThrottle();
		Task::enable();
	};

	void OnServiceSessionCreation() final
	{
		switch (AmState)
		{
		case AmStateEnum::RequestingSession:
			// Wait longer because reply is big.
			if (UnlinkedPacketThrottle())
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
						Serial.println(F("Sent Session Start Request."));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
			Task::enable();
			break;
		case AmStateEnum::ValidatingSession:
			if (Session.AddressCollision())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Local and Partner addresses match, link is impossible."));
#endif
				AmState = AmStateEnum::RequestingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Session token is cached, Linking."));
#endif
				AmState = AmStateEnum::SessionCached;
			}
			else
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Session token needs refreshing."));
#endif
				Session.ResetAm();
				AmState = AmStateEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case AmStateEnum::ComputingSecretKey:
			if (Session.Calculate(micros()))
			{
				AmState = AmStateEnum::SessionCached;
			}
			Task::enable();
			break;
		case AmStateEnum::SessionCached:
			if (Session.GetCacheAge(micros()) > GetLinkingTimeoutDuration())
			{
				ResetSearch();
			}
			else if (UnlinkedPacketThrottle())
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
						Serial.println(F("Sent Linking Start Request."));
#endif
					}
				}
				LOLA_RTOS_RESUME();
			}
			Task::enable();
			break;
		}
	}
};
#endif