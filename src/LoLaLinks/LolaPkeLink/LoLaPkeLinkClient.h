// LoLaPkeLinkClient.h

#ifndef _LOLA_PKE_LINK_CLIENT_
#define _LOLA_PKE_LINK_CLIENT_

#include "..\Abstract\AbstractLoLaLinkClient.h"

#include "..\..\Crypto\LoLaCryptoPkeSession.h"

/// <summary>
/// LoLa Public-Key-Exchange Link Client.
/// Template parameters allow specifying maxlisteners.
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaPkeLinkClient : public AbstractLoLaLinkClient<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLinkClient<MaxPacketReceiveListeners, MaxLinkListeners>;

public:
	static constexpr uint32_t CLIENT_SLEEP_TIMEOUT_MILLIS = 30000;

	static constexpr uint32_t CHANNEL_SEARCH_PERIOD_MILLIS = 7;
	static constexpr uint32_t CHANNEL_SEARCH_JITTER_MILLIS = 3;
	static constexpr uint32_t CHANNEL_SEARCH_TRY_COUNT = 3;

private:
	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;

	enum ClientAwaitingLinkEnum
	{
		SearchingServer,
		RequestingSession,
		ValidatingSession,
		ComputingSecretKey,
		TryLinking,
		SwitchingToLinking,
		Sleeping
	};

	enum ClientLinkingEnum
	{
		WaitingForAuthenticationRequest,
		AuthenticationReply,
		AuthenticationRequest,
		ClockSyncing,
		RequestingLinkStart,
		SwitchingToLinked
	};

protected:
	using BaseClass::ExpandedKey;
	using BaseClass::OutPacket;
	using BaseClass::RandomSource;
	using BaseClass::PacketService;

	using BaseClass::StateTransition;

	using BaseClass::SearchChannel;

	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;
	using BaseClass::GetStageElapsedMillis;
	using BaseClass::GetElapsedMicrosSinceLastUnlinkedSent;


private:
	LoLaCryptoPkeSession Session;

	uint8_t SearchChannelTryCount = 0;

	ClientAwaitingLinkEnum SubState = ClientAwaitingLinkEnum::Sleeping;

protected:
#if defined(DEBUG_LOLA)
	virtual void Owner() final
	{
		Serial.print(millis());
		Serial.print(F("\t[C] "));
	}
#endif

public:
	LoLaPkeLinkClient(Scheduler& scheduler,
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
	virtual void OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t counter) final
	{
		switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
		{
		case Unlinked::SearchReply::SUB_HEADER:
			if (payloadSize == Unlinked::SearchReply::PAYLOAD_SIZE)
			{
				switch (SubState)
				{
				case ClientAwaitingLinkEnum::SearchingServer:
					SubState = ClientAwaitingLinkEnum::RequestingSession;
					Task::enable();
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Found a server!"));
#endif
					break;
				case ClientAwaitingLinkEnum::RequestingSession:
					break;
				default:
					break;
				}

				Task::enable();
			}
			break;
		case Unlinked::SessionBroadcast::SUB_HEADER:
			if (payloadSize == Unlinked::SessionBroadcast::PAYLOAD_SIZE)
			{
				switch (SubState)
				{
				case ClientAwaitingLinkEnum::RequestingSession:
					SubState = ClientAwaitingLinkEnum::ValidatingSession;

#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Found a server! (session broadcast)"));
#endif
					break;
				default:
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Rejected a server! (session broadcast). Substate: "));
					Serial.println(SubState);
#endif
					return;
					break;
				}
			}

			//TODO: Validate Public Id.
			Session.SetSessionId(&payload[Unlinked::SessionBroadcast::PAYLOAD_SESSION_ID_INDEX]);

			// Slow operation (~8 ms)... but this call is already async and don't need to store the partners compresses public key.
			Session.DecompressPartnerPublicKeyFrom(&payload[Unlinked::SessionBroadcast::PAYLOAD_PUBLIC_KEY_INDEX]);
			Task::enable();
			break;
		case Unlinked::LinkingTimedSwitchOver::SUB_HEADER:
			if (payloadSize == Unlinked::LinkingTimedSwitchOver::PAYLOAD_SIZE
				&& Session.LinkingTokenMatches(&payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]))
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Got LinkingTimedSwitchOver."));
#endif
				switch (SubState)
				{
				case ClientAwaitingLinkEnum::TryLinking:
					SubState = ClientAwaitingLinkEnum::SwitchingToLinking;
					break;
				case ClientAwaitingLinkEnum::SwitchingToLinking:
					break;
				default:
					return;
					break;
				}

				StateTransition.OnReceived(startTimestamp, &payload[Unlinked::LinkingTimedSwitchOver::PAYLOAD_TIME_INDEX]);
				Task::enable();
			}
			break;
		default:
			BaseClass::OnUnlinkedPacketReceived(startTimestamp, payload, payloadSize, counter);
			break;
		}
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage) final
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::Booting:
			break;
		case LinkStageEnum::AwaitingLink:
			SubState = ClientAwaitingLinkEnum::SearchingServer;
			SearchChannelTryCount = 0;
			break;
		case LinkStageEnum::Linking:
			break;
		case LinkStageEnum::Linked:
			break;
		default:
			break;
		}
	}


	virtual void OnServiceAwaitingLink() final
	{
		if (GetStageElapsedMillis() > CLIENT_SLEEP_TIMEOUT_MILLIS)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("OnAwaitingLink timed out. Going to sleep."));
#endif
			SubState = ClientAwaitingLinkEnum::Sleeping;
		}

		switch (SubState)
		{
		case ClientAwaitingLinkEnum::SearchingServer:
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
			{
				if (SearchChannelTryCount >= CHANNEL_SEARCH_TRY_COUNT)
				{
					// Switch channels after a few ms of failed attempt.
					// Has no effect if Channel Hop is permanent.
					SearchChannel++;
					SetHopperFixedChannel(SearchChannel);
					SearchChannelTryCount = 0;
					Task::enable();
					return;
				}
				else
				{
					// Server search packet.
					OutPacket.SetPort(Unlinked::PORT);
					OutPacket.Payload[Unlinked::SearchRequest::SUB_HEADER_INDEX] = Unlinked::SearchRequest::SUB_HEADER;

					if (SendPacket(OutPacket.Data, Unlinked::SearchRequest::PAYLOAD_SIZE))
					{
						SearchChannelTryCount++;
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent Broadcast Search."));
#endif
					}
				}
			}
			break;
		case ClientAwaitingLinkEnum::RequestingSession:
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
			{
				// Session PKE start request packet.
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::SessionRequest::SUB_HEADER_INDEX] = Unlinked::SessionRequest::SUB_HEADER;

				if (SendPacket(OutPacket.Data, Unlinked::SessionRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Session Start Request."));
#endif
				}
			}
			Task::enable();
			break;
		case ClientAwaitingLinkEnum::ValidatingSession:
			if (Session.PublicKeyCollision())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Local and Partner public keys match, link is impossible."));
#endif
				SubState = ClientAwaitingLinkEnum::RequestingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session token is cached, let's start linking."));
#endif
				SubState = ClientAwaitingLinkEnum::TryLinking;
				StateTransition.OnStart();
			}
			else
			{
				Session.ResetPke();
				SubState = ClientAwaitingLinkEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case ClientAwaitingLinkEnum::ComputingSecretKey:
			if (Session.CalculatePke())
			{
				// PKE calculation took a lot of time, let's reset to start now with a cached key-pair.
				SubState = ClientAwaitingLinkEnum::ValidatingSession;
			}
			Task::enable();
			break;
		case ClientAwaitingLinkEnum::TryLinking:
			if (GetElapsedMicrosSinceLastUnlinkedSent() > LoLaLinkDefinition::RE_TRANSMIT_TIMEOUT_MICROS
				&& PacketService.CanSendPacket())
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::LinkingStartRequest::SUB_HEADER_INDEX] = Unlinked::LinkingStartRequest::SUB_HEADER;
				Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]);
				Session.CompressPublicKeyTo(&OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX]);

				if (SendPacket(OutPacket.Data, Unlinked::LinkingStartRequest::PAYLOAD_SIZE))
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Linking Start Request."));
#endif
				}
			}
			Task::enable();
			break;
		case ClientAwaitingLinkEnum::SwitchingToLinking:
			if (StateTransition.HasTimedOut(micros()))
			{
				if (StateTransition.HasAcknowledge())
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("StateTransition success."));
#endif
					UpdateLinkStage(LinkStageEnum::Linking);
				}
				else
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("StateTransition timed out."));
#endif
				}
			}
			else if (StateTransition.IsSendRequested(micros()) && PacketService.CanSendPacket())
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::SUB_HEADER_INDEX] = Unlinked::LinkingTimedSwitchOverAck::SUB_HEADER;
				Session.CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SESSION_TOKEN_INDEX]);

				if (SendPacket(OutPacket.Data, Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE))
				{
					StateTransition.OnSent(micros());
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent StateTransition Ack."));
#endif
				}
				else
				{
					Task::enableIfNot();
				}
			}
			else
			{
				Task::enableIfNot();
			}
			break;
		case ClientAwaitingLinkEnum::Sleeping:
			Task::disable();
			break;
		default:
			break;
		}
	}
};
#endif