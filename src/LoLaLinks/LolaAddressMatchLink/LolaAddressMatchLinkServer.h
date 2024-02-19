// LoLaAddressMatchLinkServer.h

#ifndef _LOLA_ADDRESS_MATCH_LINK_SERVER_
#define _LOLA_ADDRESS_MATCH_LINK_SERVER_

#include "..\Abstract\AbstractLoLaLinkServer.h"
#include "..\..\Crypto\LoLaCryptoAmSession.h"

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

public:
	static constexpr uint32_t SERVER_SLEEP_TIMEOUT_MILLIS = 10000;

private:
	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;

	enum class AmStateEnum
	{
		NoMatch,
		ValidatingSession,
		ComputingSecretKey,
		SessionCached
	};

private:
	LinkRegistry<MaxPacketReceiveListeners, MaxLinkListeners> RegistryInstance{};

	LoLaCryptoAmSession Session;

	AmStateEnum AmState = AmStateEnum::NoMatch;

	bool AmSessionRequested = false;

public:
	LoLaAddressMatchLinkServer(Scheduler& scheduler,
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
	virtual void OnUnlinkedPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize) final
	{
		switch (payload[HeaderDefinition::HEADER_INDEX])
		{
		case Unlinked::AmSessionRequest::HEADER:
			if (payloadSize == Unlinked::AmSessionRequest::PAYLOAD_SIZE)
			{
				if (IsInSearchingLink())
				{
					AmSessionRequested = true;
					Task::enable();
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Session request received."));
#endif
				}
				else if (IsInSessionCreation())
				{
					if (AmState == AmStateEnum::SessionCached)
					{
						Session.ResetAm();
						AmState = AmStateEnum::NoMatch;
						StartSearching();
						AmSessionRequested = true;
						Task::enable();
#if defined(DEBUG_LOLA_LINK)
						this->Owner();
						Serial.println(F("Session request overrode state back to search."));
#endif
					}
#if defined(DEBUG_LOLA_LINK)
					else { this->Skipped(F("SessionRequest1")); }
#endif
				}
#if defined(DEBUG_LOLA_LINK)
				else { this->Skipped(F("SessionRequest2")); }
#endif
			}
#if defined(DEBUG_LOLA_LINK)
			else { this->Skipped(F("SessionRequest3")); }
#endif
			break;
		case Unlinked::AmLinkingStartRequest::HEADER:
			if (payloadSize == Unlinked::AmLinkingStartRequest::PAYLOAD_SIZE
				&& Session.LocalAddressMatchFrom(&payload[Unlinked::AmLinkingStartRequest::PAYLOAD_SERVER_ADDRESS_INDEX])
				&& Session.SessionIdMatches(&payload[Unlinked::AmLinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]))
			{
				if (IsInSearchingLink())
				{
					StartSessionCreationIfNot();
					Task::enable();
				}
				else if (IsInSessionCreation())
				{
					switch (AmState)
					{
					case AmStateEnum::NoMatch:
					case AmStateEnum::SessionCached:
						Task::enable();
						break;
					default:
#if defined(DEBUG_LOLA_LINK)
						this->Skipped(F("LinkingStartRequest1"));
#endif
						return;
					}
				}
				else
				{
#if defined(DEBUG_LOLA_LINK)
					this->Skipped(F("LinkingStartRequest2"));
#endif
					return;
				}

				Session.SetPartnerAddressFrom(&payload[Unlinked::AmLinkingStartRequest::PAYLOAD_CLIENT_ADDRESS_INDEX]);
				AmState = AmStateEnum::ValidatingSession;
			}
#if defined(DEBUG_LOLA_LINK)
			else {
				this->Skipped(F("LinkingStartRequest3"));
			}
#endif
			break;
		default:
			BaseClass::OnUnlinkedPacketReceived(timestamp, payload, payloadSize);
			break;
		}
	}

protected:
	virtual void ResetSessionCreation() final
	{
		AmState = AmStateEnum::NoMatch;
		Session.ResetAm();
	}

	virtual void OnServiceSearchingLink()
	{
		if (AmSessionRequested)
		{
			OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
			OutPacket.SetHeader(Unlinked::AmSessionAvailable::HEADER);
			Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::AmSessionAvailable::PAYLOAD_SESSION_ID_INDEX]);
			Session.CopyLocalAddressTo(&OutPacket.Payload[Unlinked::AmSessionAvailable::PAYLOAD_SERVER_ADDRESS_INDEX]);

			LOLA_RTOS_PAUSE();
			if (UnlinkedDuplexCanSend(Unlinked::AmSessionAvailable::PAYLOAD_SIZE) &&
				PacketService.CanSendPacket())
			{
				AmSessionRequested = false;
				if (SendPacket(OutPacket.Data, Unlinked::AmSessionAvailable::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA_LINK)
					this->Owner();
					Serial.println(F("Sent Address Match Session."));
#endif
				}
			}
			LOLA_RTOS_RESUME();
		}
		Task::enable();
	}

	virtual void OnServiceSessionCreation() final
	{
		switch (AmState)
		{
		case AmStateEnum::NoMatch:
		case AmStateEnum::SessionCached:
			Task::enable();
			break;
		case AmStateEnum::ValidatingSession:
			if (Session.AddressCollision())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Local and Partner addresses match, link is impossible."));
#endif
				AmState = AmStateEnum::NoMatch;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Session secrets are cached, let's start linking."));
#endif
				StartSwitchToLinking();
			}
			else
			{
#if defined(DEBUG_LOLA_LINK)
				this->Owner();
				Serial.println(F("Session secrets invalidated, caching..."));
#endif
				Session.ResetAm();
				AmState = AmStateEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case AmStateEnum::ComputingSecretKey:
			if (Session.Calculate())
			{
				AmState = AmStateEnum::SessionCached;
			}
			Task::enable();
			break;
		default:
			break;
		}
	}
};
#endif