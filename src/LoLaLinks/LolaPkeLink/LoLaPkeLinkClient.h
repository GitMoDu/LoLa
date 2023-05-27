// LoLaPkeLinkClient.h

#ifndef _LOLA_PKE_LINK_CLIENT_
#define _LOLA_PKE_LINK_CLIENT_

#include "..\Abstract\AbstractPublicKeyLoLaLink.h"

/// <summary>
/// LoLa Public-Key-Exchange Link Client.
/// Template parameters allow specifying maxlisteners.
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaPkeLinkClient : public AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractPublicKeyLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;

public:
	static constexpr uint32_t CLIENT_SLEEP_TIMEOUT_MILLIS = 5000;

	static constexpr uint32_t CHANNEL_SEARCH_PERIOD_MILLIS = 3;
	static constexpr uint32_t CHANNEL_SEARCH_JITTER_MILLIS = 2;
	static constexpr uint32_t CHANNEL_SEARCH_TRY_COUNT = 2;

	static constexpr uint32_t CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS = 50;

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
	using BaseClass::LinkStage;
	using BaseClass::OutPacket;
	using BaseClass::SyncClock;
	using BaseClass::Session;
	using BaseClass::RandomSource;
	using BaseClass::Transceiver;

	using BaseClass::PacketService;

	using BaseClass::GetElapsedSinceLastValidReceived;
	using BaseClass::GetSendDuration;
	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;
	using BaseClass::PreLinkResendDelayMillis;

private:
	Timestamp OutEstimate{};
	TimestampError EstimateErrorReply{};

	ClientTimedStateTransition<
		LoLaLinkDefinition::LINKING_TRANSITION_PERIOD_MICROS,
		LoLaLinkDefinition::LINKING_TRANSITION_RESEND_PERIOD_MICROS>
		StateTransition;


	uint32_t PreLinkPacketSchedule = 0;
	uint8_t SubState = 0;

	uint8_t SearchChannel = 0;
	uint8_t SearchChannelTryCount = 0;
	uint8_t LastKnownBroadCastChannel = INT8_MAX + 1;

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
		: BaseClass(scheduler, transceiver, entropySource, clockSource, timerSource, duplex, hop, publicKey, privateKey, accessPassword)
	{}

#pragma region Packet Handling
protected:
	virtual void OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port, const uint8_t counter)
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
						switch (SubState)
						{
						case ClientAwaitingLinkEnum::SearchingServer:
							SubState = (uint8_t)ClientAwaitingLinkEnum::RequestingSession;
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

						PreLinkPacketSchedule = millis();
						Task::enable();
					}
					break;
				case Unlinked::SessionBroadcast::SUB_HEADER:
					if (payloadSize == Unlinked::SessionBroadcast::PAYLOAD_SIZE)
					{
						switch (SubState)
						{
						case ClientAwaitingLinkEnum::RequestingSession:
							SubState = (uint8_t)ClientAwaitingLinkEnum::ValidatingSession;
							PreLinkPacketSchedule = millis();

#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Found a server! (session broadcast)"));
#endif
							break;
						default:
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Rejected a server! (session broadcast)"));
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
					break;
				}
			}
			break;
		case Linking::PORT:
			if (LinkStage == LinkStageEnum::Linking)
			{
				switch (payload[Linking::ServerChallengeRequest::SUB_HEADER_INDEX])
				{
				case Linking::ServerChallengeRequest::SUB_HEADER:
					if (payloadSize == Linking::ServerChallengeRequest::PAYLOAD_SIZE)
					{
						switch (SubState)
						{
						case ClientLinkingEnum::WaitingForAuthenticationRequest:
							Session.SetPartnerChallenge(&payload[Linking::ServerChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);
							SubState = ClientLinkingEnum::AuthenticationReply;
							PreLinkPacketSchedule = millis();
							Task::enable();
							break;
						case ClientLinkingEnum::AuthenticationReply:
							break;
						default:
							break;
						}
					}
#if defined(DEBUG_LOLA)
					else
					{
						this->Owner();
						Serial.println(F("ServerChallengeRequest rejected."));
					}
#endif
					break;
				case Linking::ServerChallengeReply::SUB_HEADER:
					if (payloadSize == Linking::ServerChallengeReply::PAYLOAD_SIZE)
					{
						switch (SubState)
						{
						case ClientLinkingEnum::AuthenticationReply:
							if (Session.VerifyChallengeSignature(&payload[Linking::ServerChallengeReply::PAYLOAD_SIGNED_INDEX]))
							{
#if defined(DEBUG_LOLA)
								this->Owner();
								Serial.println(F("Link Access Control granted."));
#endif
								PreLinkPacketSchedule = millis();
								SubState = ClientLinkingEnum::ClockSyncing;
								Task::enable();
							}
							break;
						case ClientLinkingEnum::ClockSyncing:
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
				case Linking::ClockSyncReply::SUB_HEADER:
					if (payloadSize == Linking::ClockSyncReply::PAYLOAD_SIZE
						&& SubState == ClientLinkingEnum::ClockSyncing)
					{
						EstimateErrorReply.Seconds = payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX];
						EstimateErrorReply.Seconds += (uint_least16_t)payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 1] << 8;
						EstimateErrorReply.Seconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 2] << 16;
						EstimateErrorReply.Seconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SECONDS_INDEX + 3] << 24;
						EstimateErrorReply.SubSeconds = payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX];
						EstimateErrorReply.SubSeconds += (uint_least16_t)payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 1] << 8;
						EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 2] << 16;
						EstimateErrorReply.SubSeconds += (uint32_t)payload[Linking::ClockSyncReply::PAYLOAD_SUB_SECONDS_INDEX + 3] << 24;

						if (!EstimateErrorReply.Validate())
						{
#if defined(DEBUG_LOLA)
							// Invalid estimate, sub-seconds should never match one second.
							this->Owner();
							Serial.print(F("Clock Estimate Error Invalid. SubSeconds="));
							Serial.print(EstimateErrorReply.SubSeconds);
							Serial.println(F("us. "));
#endif
						}
						// Adjust local clock to match estimation error.
						SyncClock.ShiftSeconds(EstimateErrorReply.Seconds);
						SyncClock.ShiftMicros(EstimateErrorReply.SubSeconds);

						if (payload[Linking::ClockSyncReply::PAYLOAD_ACCEPTED_INDEX] > 0)
						{
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Clock Accepted, starting final step."));
#endif
							SubState = ClientLinkingEnum::RequestingLinkStart;
							StateTransition.OnStart();
							PreLinkPacketSchedule = millis();
						}
						else
						{
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Clock Rejected, trying again."));
#endif
						}

						PreLinkPacketSchedule = millis();
						Task::enable();
					}
					break;
				case Linking::LinkTimedSwitchOver::SUB_HEADER:
					if (payloadSize == Linking::LinkTimedSwitchOver::PAYLOAD_SIZE)
					{

						switch (SubState)
						{
						case ClientLinkingEnum::RequestingLinkStart:
							SubState = (uint8_t)ClientLinkingEnum::SwitchingToLinked;
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.print(F("StateTransition Client received, started with"));
							Serial.print(StateTransition.GetDurationUntilTimeOut(startTimestamp));
							Serial.println(F("us remaining."));
#endif
							break;
						case ClientLinkingEnum::SwitchingToLinked:
							break;
						default:
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("LinkSwitchOver rejected."));
#endif
							return;
							break;
						}
						StateTransition.OnReceived(startTimestamp, &payload[Linking::LinkTimedSwitchOver::PAYLOAD_TIME_INDEX]);

						Task::enable();
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
			Session.SetRandomSessionId(&RandomSource);
			SyncClock.ShiftSeconds(RandomSource.GetRandomLong());
			LastKnownBroadCastChannel = RandomSource.GetRandomShort();
			break;
		case LinkStageEnum::AwaitingLink:
			SubState = (uint8_t)ClientAwaitingLinkEnum::SearchingServer;
			SearchChannel = LastKnownBroadCastChannel;
			SearchChannelTryCount = 0;
			SetHopperFixedChannel(SearchChannel);
			break;
		case LinkStageEnum::Linking:
			SubState = (uint8_t)ClientLinkingEnum::WaitingForAuthenticationRequest;
			break;
		case LinkStageEnum::Linked:
			LastKnownBroadCastChannel = SearchChannel;
			break;
		default:
			break;
		}
	}


	virtual void OnAwaitingLink(const uint32_t stateElapsed) final
	{
		if (stateElapsed > CLIENT_SLEEP_TIMEOUT_MILLIS)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("OnAwaitingLink timed out. Going to sleep."));
#endif
			SubState = (uint8_t)ClientAwaitingLinkEnum::Sleeping;
		}

		switch (SubState)
		{
		case ClientAwaitingLinkEnum::SearchingServer:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
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
					// Server search packet.
					OutPacket.SetPort(Unlinked::PORT);
					OutPacket.Payload[Unlinked::SearchRequest::SUB_HEADER_INDEX] = Unlinked::SearchRequest::SUB_HEADER;

#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sending Broadcast Search."));
#endif
					if (SendPacket(OutPacket.Data, Unlinked::SearchRequest::PAYLOAD_SIZE))
					{
						PreLinkPacketSchedule = millis() + CHANNEL_SEARCH_PERIOD_MILLIS + RandomSource.GetRandomShort(CHANNEL_SEARCH_JITTER_MILLIS);
						SearchChannelTryCount++;
					}
				}
			}
			break;
		case ClientAwaitingLinkEnum::RequestingSession:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				// Session PKE start request packet.
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::SessionRequest::SUB_HEADER_INDEX] = Unlinked::SessionRequest::SUB_HEADER;

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending Session Start Request."));
#endif
				if (SendPacket(OutPacket.Data, Unlinked::SessionRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionRequest::PAYLOAD_SIZE);
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
				SubState = (uint8_t)ClientAwaitingLinkEnum::RequestingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session token is cached, let's start linking."));
#endif
				SubState = (uint8_t)ClientAwaitingLinkEnum::TryLinking;
				StateTransition.OnStart();
				PreLinkPacketSchedule = millis();
			}
			else
			{
				Session.ResetPke();
				SubState = (uint8_t)ClientAwaitingLinkEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case ClientAwaitingLinkEnum::ComputingSecretKey:
			if (Session.CalculatePke())
			{
				// PKE calculation took a lot of time, let's reset to start now with a cached key-pair.
				SubState = (uint8_t)ClientAwaitingLinkEnum::ValidatingSession;
				PreLinkPacketSchedule = millis();
			}
			Task::enable();
			break;
		case ClientAwaitingLinkEnum::TryLinking:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::LinkingStartRequest::SUB_HEADER_INDEX] = Unlinked::LinkingStartRequest::SUB_HEADER;
				Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]);
				Session.CompressPublicKeyTo(&OutPacket.Payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX]);

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending Linking Start Request."));
#endif
				if (SendPacket(OutPacket.Data, Unlinked::LinkingStartRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::LinkingStartRequest::PAYLOAD_SIZE);
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
					SubState = (uint8_t)ClientAwaitingLinkEnum::TryLinking;
					PreLinkPacketSchedule = millis();
				}
			}
			else if (Transceiver->TxAvailable() && StateTransition.IsSendRequested(micros()))
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::SUB_HEADER_INDEX] = Unlinked::LinkingTimedSwitchOverAck::SUB_HEADER;
				Session.CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SESSION_TOKEN_INDEX]);

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending StateTransition Ack."));
#endif
				if (SendPacket(OutPacket.Data, Unlinked::LinkingTimedSwitchOverAck::PAYLOAD_SIZE))
				{
					StateTransition.OnSent(micros());
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

		switch (SubState)
		{
		case ClientLinkingEnum::WaitingForAuthenticationRequest:
			// Server will be the one initiating the authentication step.
			if (stateElapsed > CLIENT_AUTH_REQUEST_WAIT_TIMEOUT_MILLIS)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("WaitingForAuthenticationRequest timed out!"));
#endif
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else
			{
				Task::enableDelayed(1);
			}
			break;
		case ClientLinkingEnum::AuthenticationReply:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::ClientChallengeReplyRequest::SUB_HEADER_INDEX] = Linking::ClientChallengeReplyRequest::SUB_HEADER;
				Session.SignPartnerChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]);
				Session.CopyLocalChallengeTo(&OutPacket.Payload[Linking::ClientChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending ClientChallengeReplyRequest."));
#endif

				if (SendPacket(OutPacket.Data, Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::ClientChallengeReplyRequest::PAYLOAD_SIZE);
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
		case ClientLinkingEnum::ClockSyncing:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::ClockSyncRequest::SUB_HEADER_INDEX] = Linking::ClockSyncRequest::SUB_HEADER;

				SyncClock.GetTimestamp(OutEstimate);
				OutEstimate.ShiftSubSeconds((int32_t)GetSendDuration(Linking::ClockSyncRequest::PAYLOAD_SIZE));

				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX] = OutEstimate.Seconds;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 1] = OutEstimate.Seconds >> 8;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 2] = OutEstimate.Seconds >> 16;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SECONDS_INDEX + 3] = OutEstimate.Seconds >> 24;

				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX] = OutEstimate.SubSeconds;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 1] = OutEstimate.SubSeconds >> 8;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 2] = OutEstimate.SubSeconds >> 16;
				OutPacket.Payload[Linking::ClockSyncRequest::PAYLOAD_SUB_SECONDS_INDEX + 3] = OutEstimate.SubSeconds >> 24;

#if defined(DEBUG_LOLA)
				//this->Owner();
				//Serial.print(F("Sending ClockSync "));
				//Serial.print(OutEstimate.Seconds);
				//Serial.println('s');
#endif
				if (SendPacket(OutPacket.Data, Linking::ClockSyncRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::ClockSyncRequest::PAYLOAD_SIZE);
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
		case ClientLinkingEnum::RequestingLinkStart:
			if (Transceiver->TxAvailable() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::StartLinkRequest::SUB_HEADER_INDEX] = Linking::StartLinkRequest::SUB_HEADER;

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending Linking Start Request."));
#endif
				if (SendPacket(OutPacket.Data, Linking::StartLinkRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::StartLinkRequest::PAYLOAD_SIZE);
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
		case ClientLinkingEnum::SwitchingToLinked:
			if (StateTransition.HasTimedOut(micros()))
			{
				if (StateTransition.HasAcknowledge())
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("StateTransition success."));
#endif
					UpdateLinkStage(LinkStageEnum::Linked);
				}
				else
				{
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("StateTransition timed out."));
#endif
					UpdateLinkStage(LinkStageEnum::AwaitingLink);
				}
			}
			else if (Transceiver->TxAvailable() && StateTransition.IsSendRequested(micros()))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::LinkTimedSwitchOverAck::SUB_HEADER_INDEX] = Linking::LinkTimedSwitchOverAck::SUB_HEADER;

#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Sending StateTransition Ack."));
#endif
				if (SendPacket(OutPacket.Data, Linking::LinkTimedSwitchOverAck::PAYLOAD_SIZE))
				{
					StateTransition.OnSent(micros());
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
		default:
			break;
		}
	}
#pragma endregion


	//bool PendingMicrosSend = false;
	//uint32_t LastClockSync = 0;

	//static constexpr uint32_t CLOCK_SYNC_UPDATE_PERIOD_MILLIS = 2000;


#if defined(CLIENT_DROP_LINK_TEST)
	virtual const bool CheckForClockSyncUpdate() final
	{
		if (this->GetLinkDuration() > CLIENT_DROP_LINK_TEST)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.print(("Test disconnect after "));
			Serial.print(CLIENT_DROP_LINK_TEST);
			Serial.println((" seconds."));
#endif
			UpdateLinkStage(LinkStageEnum::AwaitingLink);
		}

		return false;
	}
#endif
};
#endif