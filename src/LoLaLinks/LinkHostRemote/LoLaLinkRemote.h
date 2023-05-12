// LoLaLinkRemote.h

#ifndef _LOLA_LINK_REMOTE_
#define _LOLA_LINK_REMOTE_

#include "..\Abstract\AbstractLoLaLink.h"

/// <summary>
/// Link Remote Template.
/// </summary>
/// <typeparam name="MaxPayloadSize"></typeparam>
/// <typeparam name="MaxPayloadLinkSend"></typeparam>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaLinkRemote : public AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;

public:
	static constexpr uint32_t REMOTE_SLEEP_TIMEOUT_MILLIS = 5000;

	static constexpr uint32_t CHANNEL_SEARCH_PERIOD_MILLIS = 3;
	static constexpr uint32_t CHANNEL_SEARCH_JITTER_MILLIS = 2;
	static constexpr uint32_t CHANNEL_SEARCH_TRY_COUNT = 2;


	static constexpr uint32_t REMOTE_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS = 50;

private:
	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;

	enum RemoteAwaitingLinkEnum
	{
		SearchingHost,
		RequestingSession,
		ValidatingSession,
		ComputingSecretKey,
		TryLinking,
		SwitchingToLinking,
		Sleeping
	};

	enum RemoteLinkingEnum
	{
		WaitingForAuthenticationRequest,
		AuthenticationReply,
		AuthenticationRequest,
		ClockSyncing,
		SwitchingToLinked
	};


protected:
	using BaseClass::LinkStage;
	using BaseClass::OutPacket;
	using BaseClass::SyncClock;
	using BaseClass::Session;

	using BaseClass::PacketService;

	using BaseClass::GetElapsedSinceLastValidReceived;
	using BaseClass::GetSendDuration;
	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;
	using BaseClass::PreLinkResendDelayMillis;

private:
	Timestamp OutEstimate;
	TimestampError EstimateErrorReply;



	uint32_t PreLinkPacketSchedule = 0;
	uint8_t AwaitingLinkSubState = 0;
	uint8_t LinkingSubState = 0;

	uint8_t SwitchOverHandle = 0;
	uint8_t LooseHandle = 0;
	uint8_t TimeHandle = 0;


	uint8_t SearchChannel = 0;
	uint8_t SearchChannelTryCount = 0;
	uint8_t LastKnownBroadCastChannel = INT8_MAX + 1;

protected:
#if defined(DEBUG_LOLA)
	virtual void Owner() final
	{
		Serial.print(millis());
		Serial.print(F("\t[R] "));
	}
#endif

public:
	LoLaLinkRemote(Scheduler& scheduler,
		ILoLaPacketDriver* driver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop,
		const uint8_t* publicKey,
		const uint8_t* privateKey,
		const uint8_t* accessPassword)
		: BaseClass(scheduler, driver, entropySource, clockSource, timerSource, duplex, hop, publicKey, privateKey, accessPassword)
	{}

#pragma region Packet Handling
protected:
	virtual void OnAckSent(const uint8_t port, const uint8_t id) final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::AwaitingLink:
			if (port == Unlinked::PORT
				&& AwaitingLinkSubState == (uint8_t)RemoteAwaitingLinkEnum::TryLinking
				&& SwitchOverHandle == id)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("On Ack Send Linking!"));
#endif

				AwaitingLinkSubState = RemoteAwaitingLinkEnum::SwitchingToLinking;
				Task::enable();
			}
			break;
		case LinkStageEnum::Linking:
			if (port == Linking::PORT
				&& LinkingSubState == (uint8_t)RemoteLinkingEnum::ClockSyncing
				&& SwitchOverHandle == id)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("On Ack Send Linked!"));
#endif
				LinkingSubState = RemoteLinkingEnum::SwitchingToLinked;
				Task::enable();
			}
			break;
		default:
			break;
		}
	}

	virtual const bool OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port, const uint8_t counter)
	{
		switch (port)
		{
		case Unlinked::PORT:
			if (LinkStage == LinkStageEnum::AwaitingLink)
			{
				switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
				{
				case Unlinked::SearchReply::SUB_HEADER:
					if (payloadSize == Unlinked::SearchReply::PAYLOAD_SIZE)
					{
						switch (AwaitingLinkSubState)
						{
						case RemoteAwaitingLinkEnum::SearchingHost:
							AwaitingLinkSubState = (uint8_t)RemoteAwaitingLinkEnum::RequestingSession;
							Task::enable();
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Found a host!"));
#endif
							break;
						case RemoteAwaitingLinkEnum::RequestingSession:
							return false;
						default:
							return false;
						}

						PreLinkPacketSchedule = millis();
						Task::enable();
					}
					break;
				case Unlinked::SessionBroadcast::SUB_HEADER:
					if (payloadSize == Unlinked::SessionBroadcast::PAYLOAD_SIZE)
					{
						switch (AwaitingLinkSubState)
						{
						case RemoteAwaitingLinkEnum::SearchingHost:
							AwaitingLinkSubState = (uint8_t)RemoteAwaitingLinkEnum::RequestingSession;
							PreLinkPacketSchedule = millis();
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Found a host! (session broadcast)"));
#endif
							break;
						case RemoteAwaitingLinkEnum::RequestingSession:
							break;
						default:
							return false;
						}
					}

					//TODO: Validate Public Id.
					Session.SetSessionId(&payload[Unlinked::SessionBroadcast::PAYLOAD_SESSION_ID_INDEX]);

					// Slow operation (~8 ms)... but this call is already async and don't need to store the partners compresses public key.
					Session.DecompressPartnerPublicKeyFrom(&payload[Unlinked::SessionBroadcast::PAYLOAD_PUBLIC_KEY_INDEX]);

					AwaitingLinkSubState = (uint8_t)RemoteAwaitingLinkEnum::ValidatingSession;

					Task::enable();
					break;
				case Unlinked::LinkingSwitchOver::SUB_HEADER:
					if (payloadSize == Unlinked::LinkingSwitchOver::PAYLOAD_SIZE
						&& AwaitingLinkSubState == (uint8_t)RemoteAwaitingLinkEnum::TryLinking)
					{
						if (Session.LinkingTokenMatches(&payload[Unlinked::LinkingSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]))
						{
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Linking switch accepted."));
#endif
							Task::enable();
							SwitchOverHandle = counter;
							return true;
						}
					}
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Linking switch rejected."));
#endif
					break;
				default:
					break;
				}
			}
			break;
		case Linking::PORT:
			if (LinkStage == LinkStageEnum::Linking)
			{
				switch (payload[Linking::HostChallengeRequest::SUB_HEADER_INDEX])
				{
				case Linking::HostChallengeRequest::SUB_HEADER:
					if (payloadSize == Linking::HostChallengeRequest::PAYLOAD_SIZE)
					{
						switch (LinkingSubState)
						{
						case RemoteLinkingEnum::WaitingForAuthenticationRequest:
							Session.SetPartnerChallenge(&payload[Linking::HostChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);
							LinkingSubState = RemoteLinkingEnum::AuthenticationReply;
							PreLinkPacketSchedule = millis();
							Task::enable();
							break;
						case RemoteLinkingEnum::AuthenticationReply:
							break;
						default:
							break;
						}
					}
#if defined(DEBUG_LOLA)
					else
					{
						this->Owner();
						Serial.println(F("HostChallengeRequest rejected."));
					}
#endif
					break;
				case Linking::HostChallengeReply::SUB_HEADER:
					if (payloadSize == Linking::HostChallengeReply::PAYLOAD_SIZE)
					{
						switch (LinkingSubState)
						{
						case RemoteLinkingEnum::AuthenticationReply:
							if (Session.VerifyChallengeSignature(&payload[Linking::HostChallengeReply::PAYLOAD_SIGNED_INDEX]))
							{
#if defined(DEBUG_LOLA)
								this->Owner();
								Serial.println(F("Link Access Control granted."));
#endif
								PreLinkPacketSchedule = millis();
								LinkingSubState = RemoteLinkingEnum::ClockSyncing;
								Task::enable();
							}
							break;
						case RemoteLinkingEnum::ClockSyncing:
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Unexpected Access Control response."));
#endif
							break;
						default:
							break;
						}
					}
					break;
				case Linking::LinkStartDeniedReply::SUB_HEADER:
					if (payloadSize == Linking::LinkStartDeniedReply::PAYLOAD_SIZE &&
						LinkingSubState == RemoteLinkingEnum::ClockSyncing)
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Got ClockSyncSecondsReply."));
#endif
						EstimateErrorReply.Seconds = payload[Linking::LinkStartDeniedReply::PAYLOAD_SECONDS_INDEX];
						EstimateErrorReply.Seconds += (uint_least16_t)payload[Linking::LinkStartDeniedReply::PAYLOAD_SECONDS_INDEX + 1] << 8;
						EstimateErrorReply.Seconds += (uint32_t)payload[Linking::LinkStartDeniedReply::PAYLOAD_SECONDS_INDEX + 2] << 16;
						EstimateErrorReply.Seconds += (uint32_t)payload[Linking::LinkStartDeniedReply::PAYLOAD_SECONDS_INDEX + 3] << 24;
						EstimateErrorReply.SubSeconds = payload[Linking::LinkStartDeniedReply::PAYLOAD_SUB_SECONDS_INDEX];
						EstimateErrorReply.SubSeconds += (uint_least16_t)payload[Linking::LinkStartDeniedReply::PAYLOAD_SUB_SECONDS_INDEX + 1] << 8;
						EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::LinkStartDeniedReply::PAYLOAD_SUB_SECONDS_INDEX + 2] << 16;
						EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::LinkStartDeniedReply::PAYLOAD_SUB_SECONDS_INDEX + 3] << 24;

#if defined(DEBUG_LOLA)
						if (!EstimateErrorReply.Validate())
						{
							// Invalid estimate, sub-seconds should never match one second.
							this->Owner();
							Serial.print(F("Clock Estimate Error Invalid. SubSeconds="));
							Serial.print(EstimateErrorReply.SubSeconds);
							Serial.println(F("us. "));
							return false;
						}
#endif

#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.print(F("Got Error: "));
						Serial.print(EstimateErrorReply.Seconds);
						Serial.print('s');
						if (EstimateErrorReply.SubSeconds >= 0) {
							Serial.print('+');
						}
						Serial.print(EstimateErrorReply.SubSeconds);
						Serial.println(F("us"));
#endif
						// Adjust local clock to match estimation error.
						SyncClock.ShiftSeconds(EstimateErrorReply.Seconds);
						SyncClock.ShiftMicros(EstimateErrorReply.SubSeconds);


						PreLinkPacketSchedule = millis();
						Task::enable();
					}
					break;
				case Linking::LinkSwitchOver::SUB_HEADER:
					if (payloadSize == Linking::LinkSwitchOver::PAYLOAD_SIZE &&
						LinkingSubState == RemoteLinkingEnum::ClockSyncing)
					{
						SwitchOverHandle = counter;

						EstimateErrorReply.Seconds = 0;
						EstimateErrorReply.SubSeconds = payload[Linking::LinkSwitchOver::PAYLOAD_SUB_SECONDS_INDEX];
						EstimateErrorReply.SubSeconds += (uint_least16_t)payload[Linking::LinkSwitchOver::PAYLOAD_SUB_SECONDS_INDEX + 1] << 8;
						EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::LinkSwitchOver::PAYLOAD_SUB_SECONDS_INDEX + 2] << 16;
						EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::LinkSwitchOver::PAYLOAD_SUB_SECONDS_INDEX + 3] << 24;

						// Last moment fine tune adjustment, only halfway to error, to minimize jitter.
						SyncClock.ShiftMicros(EstimateErrorReply.ErrorMicros() / 2);

#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.print(F("Got LinkSwitchOver. ("));
						Serial.print(EstimateErrorReply.SubSeconds);
						Serial.println(F(" us error)"));
#endif

						return true;
					}
#if defined(DEBUG_LOLA)
					else
					{
						this->Owner();
						Serial.println(F("LinkSwitchOver rejected."));
					}
#endif
					break;
				default:
					break;
				}
			}
			break;
		default:
			break;
		}

		return false;
	}


#pragma endregion

#pragma region Linking
protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage) final
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::Booting:
			Session.SetRandomSessionId();
			SyncClock.ShiftSeconds(Session.RandomSource.GetRandomLong());
			LastKnownBroadCastChannel = Session.RandomSource.GetRandomShort();
			break;
		case LinkStageEnum::AwaitingLink:
			AwaitingLinkSubState = (uint8_t)RemoteAwaitingLinkEnum::SearchingHost;
			SearchChannel = LastKnownBroadCastChannel;
			SearchChannelTryCount = 0;
			SetHopperFixedChannel(SearchChannel);
			break;
		//case LinkStageEnum::TransitionToLinking:
		//	break;
		case LinkStageEnum::Linking:
			LinkingSubState = (uint8_t)RemoteLinkingEnum::WaitingForAuthenticationRequest;
			break;
		//case LinkStageEnum::TransitionToLinked:
		//	break;
		case LinkStageEnum::Linked:
			LastKnownBroadCastChannel = SearchChannel;
			break;
		default:
			break;
		}
	}


	virtual void OnAwaitingLink(const uint32_t stateElapsed) final
	{
		if (stateElapsed > REMOTE_SLEEP_TIMEOUT_MILLIS)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("OnAwaitingLink timed out. Going to sleep."));
#endif
			AwaitingLinkSubState = (uint8_t)RemoteAwaitingLinkEnum::Sleeping;
		}

		switch (AwaitingLinkSubState)
		{
		case RemoteAwaitingLinkEnum::SearchingHost:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				if (SearchChannelTryCount >= CHANNEL_SEARCH_TRY_COUNT)
				{
					// Switch channels after a few ms of failed attempt.
					// Has no effect Channel Hop is permanent.
					SearchChannel++;
					SetHopperFixedChannel(SearchChannel);
					SearchChannelTryCount = 0;
					PreLinkPacketSchedule = millis();
					Task::enable();
					return;
				}
				else
				{
					// Host search packet.
					OutPacket.SetPort(Unlinked::PORT);
					OutPacket.Payload[Unlinked::SearchRequest::SUB_HEADER_INDEX] = Unlinked::SearchRequest::SUB_HEADER;

					// TODO: Add support for search for specific Host Id.
					if (SendPacket(this, LooseHandle, OutPacket.Data, Unlinked::SearchRequest::PAYLOAD_SIZE))
					{
						PreLinkPacketSchedule = millis() + CHANNEL_SEARCH_PERIOD_MILLIS + Session.RandomSource.GetRandomShort(CHANNEL_SEARCH_JITTER_MILLIS);
						SearchChannelTryCount++;
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent Broadcast Search."));
#endif
					}
				}
			}
			break;
		case RemoteAwaitingLinkEnum::RequestingSession:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				// Session PKE start request packet.
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::SessionRequest::SUB_HEADER_INDEX] = Unlinked::SessionRequest::SUB_HEADER;

				if (SendPacket(this, LooseHandle, OutPacket.Data, Unlinked::SessionRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionRequest::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Session Start Request."));
#endif
				}
			}
			Task::enable();
			break;
		case RemoteAwaitingLinkEnum::ValidatingSession:
			if (Session.PublicKeyCollision())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Local and Partner public keys match, link is impossible."));
#endif
				AwaitingLinkSubState = (uint8_t)RemoteAwaitingLinkEnum::RequestingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session token is cached, let's start linking."));
#endif
				AwaitingLinkSubState = (uint8_t)RemoteAwaitingLinkEnum::TryLinking;
				PreLinkPacketSchedule = millis();
			}
			else
			{
				Session.ResetPke();
				AwaitingLinkSubState = (uint8_t)RemoteAwaitingLinkEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case RemoteAwaitingLinkEnum::ComputingSecretKey:
			if (Session.CalculatePke()) {
				// PKE calculation took a lot of time, let's reset to start now with a cached key-pair.
				AwaitingLinkSubState = (uint8_t)RemoteAwaitingLinkEnum::RequestingSession;
				PreLinkPacketSchedule = millis();
			}
			Task::enable();
			break;
		case RemoteAwaitingLinkEnum::TryLinking:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::LinkingStartRequest::SUB_HEADER_INDEX] = Unlinked::LinkingStartRequest::SUB_HEADER;
				Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]);
				Session.CompressPublicKeyTo(&OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX]);

				if (SendPacket(this, SwitchOverHandle, OutPacket.Data, Unlinked::LinkingStartRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::LinkingStartRequest::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Link Start Request."));
#endif
				}
			}
			Task::enable();
			break;
		case RemoteAwaitingLinkEnum::SwitchingToLinking:
			UpdateLinkStage(LinkStageEnum::Linking);
			break;
		case RemoteAwaitingLinkEnum::Sleeping:
			Task::disable();
			break;
		default:
			break;
		}
	}


	virtual void OnLinking(const uint32_t stateElapsed) final
	{
		if (stateElapsed > LoLaLinkDefinition::LINKING_STAGE_TIMEOUT)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("Linking timed out!"));
#endif
			UpdateLinkStage(LinkStageEnum::AwaitingLink);
		}

		switch (LinkingSubState)
		{
		case RemoteLinkingEnum::WaitingForAuthenticationRequest:
			// Host will be the one initiatin the authentication step
			if (stateElapsed > REMOTE_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("WaitingForAuthenticationRequestd timed out!"));
#endif
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else
			{
				Task::enableDelayed(1);
			}
			break;
		case RemoteLinkingEnum::AuthenticationReply:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::RemoteChallengeReplyRequest::SUB_HEADER_INDEX] = Linking::RemoteChallengeReplyRequest::SUB_HEADER;
				Session.SignPartnerChallengeTo(&OutPacket.Payload[Linking::RemoteChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]);
				Session.CopyLocalChallengeTo(&OutPacket.Payload[Linking::RemoteChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);

				if (SendPacket(this, LooseHandle, OutPacket.Data, Linking::RemoteChallengeReplyRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::RemoteChallengeReplyRequest::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent RemoteChallengeReplyRequest."));
#endif
				}
				else
				{
					Task::enable();
				}
			}
			else
			{
				Task::enableDelayed(1);
			}
			break;
		case RemoteLinkingEnum::ClockSyncing:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::LinkStartRequest::SUB_HEADER_INDEX] = Linking::LinkStartRequest::SUB_HEADER;

				SyncClock.GetTimestamp(OutEstimate);
				OutEstimate.ShiftSubSeconds((int32_t)GetSendDuration(Linking::LinkStartRequest::PAYLOAD_SIZE));

				OutPacket.Payload[Linking::LinkStartRequest::PAYLOAD_SECONDS_INDEX] = OutEstimate.Seconds;
				OutPacket.Payload[Linking::LinkStartRequest::PAYLOAD_SECONDS_INDEX + 1] = OutEstimate.Seconds >> 8;
				OutPacket.Payload[Linking::LinkStartRequest::PAYLOAD_SECONDS_INDEX + 2] = OutEstimate.Seconds >> 16;
				OutPacket.Payload[Linking::LinkStartRequest::PAYLOAD_SECONDS_INDEX + 3] = OutEstimate.Seconds >> 24;

				OutPacket.Payload[Linking::LinkStartRequest::PAYLOAD_SUB_SECONDS_INDEX] = OutEstimate.SubSeconds;
				OutPacket.Payload[Linking::LinkStartRequest::PAYLOAD_SUB_SECONDS_INDEX + 1] = OutEstimate.SubSeconds >> 8;
				OutPacket.Payload[Linking::LinkStartRequest::PAYLOAD_SUB_SECONDS_INDEX + 2] = OutEstimate.SubSeconds >> 16;
				OutPacket.Payload[Linking::LinkStartRequest::PAYLOAD_SUB_SECONDS_INDEX + 3] = OutEstimate.SubSeconds >> 24;

				if (SendPacket(this, SwitchOverHandle, OutPacket.Data, Linking::LinkStartRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::LinkStartRequest::PAYLOAD_SIZE);


#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.print(F("Sent ClockSync "));
					if (OutEstimate.Validate())
					{
						Serial.print(F("(Valid): "));
					}
					else
					{
						Serial.print(F("(Invalid): "));
					}
					Serial.print(OutEstimate.Seconds);
					Serial.println('s');
#endif
				}
				else
				{
					Task::enable();
				}
			}
			else
			{
				Task::enableDelayed(1);
			}
			break;
		case RemoteLinkingEnum::SwitchingToLinked:
			UpdateLinkStage(LinkStageEnum::Linked);
			break;
		default:
			break;
		}
	}
#pragma endregion


	bool PendingMicrosSend = false;
	uint32_t LastClockSync = 0;

	static constexpr uint32_t CLOCK_SYNC_UPDATE_PERIOD_MILLIS = 2000;


	//virtual const bool CheckForClockSyncUpdate() final
	//{
	//	return false;
	//}
};
#endif