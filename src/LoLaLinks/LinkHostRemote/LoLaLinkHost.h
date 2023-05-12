// LoLaLinkHost.h

#ifndef _LOLA_LINK_HOST_
#define _LOLA_LINK_HOST_

#include "..\Abstract\AbstractLoLaLink.h"


/// <summary>
/// LoLa Host Link class.
/// Template parameters allow specifying maxlisteners.
/// </summary>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class LoLaLinkHost : public AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLink<MaxPacketReceiveListeners, MaxLinkListeners>;

public:
	static constexpr uint32_t HOST_SLEEP_TIMEOUT_MILLIS = 5000;

private:
	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;


	enum HostAwaitingLinkEnum
	{
		NewSessionStart,
		BroadcastingSession,
		ValidatingSession,
		ComputingSecretKey,
		SwitchingToLinking,
		Sleeping
	};

	enum HostLinkingEnum
	{
		AuthenticationRequest,
		AuthenticationReply,
		ClockSyncing,
		SwitchingToLinked
	};

protected:
	using BaseClass::LinkStage;
	using BaseClass::OutPacket;
	using BaseClass::SyncClock;
	using BaseClass::Session;

	using BaseClass::PacketService;

	using BaseClass::ResetStageStartTime;
	using BaseClass::PreLinkResendDelayMillis;
	using BaseClass::GetSendDuration;
	using BaseClass::SendPacket;
	using BaseClass::SetHopperFixedChannel;
	using BaseClass::SendPacketWithAck;


private:
	uint32_t PreLinkPacketSchedule = 0;


	Timestamp InTimestamp;
	Timestamp InEstimate;
	TimestampError EstimateErrorReply;

	volatile uint8_t SubState = 0;

	uint8_t LooseHandle = 0;
	uint8_t SwitchOverHandle = 0;

	bool SearchReplyPending = false;
	bool ClockReplyPending = false;

protected:
#if defined(DEBUG_LOLA)
	virtual void Owner() final
	{
		Serial.print(millis());
		Serial.print(F("\t[H] "));
	}
#endif

#if defined(HOST_DROP_LINK_TEST)
	virtual const bool CheckForClockSyncUpdate() final
	{
		if (this->GetLinkDuration() > HOST_DROP_LINK_TEST)
		{
			Serial.print(("[H] Test disconnect after "));
			Serial.print(HOST_DROP_LINK_TEST);
			Serial.println((" seconds."));
			UpdateLinkStage(LinkStageEnum::AwaitingLink);
		}

		return false;
	}
#endif

public:
	LoLaLinkHost(Scheduler& scheduler,
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
public:
#pragma region Packet Handling
	virtual void OnPacketAckReceived(const uint32_t startTimestamp, const uint8_t handle) final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::AwaitingLink:
			if (SubState == (uint8_t)HostAwaitingLinkEnum::SwitchingToLinking
				&& SwitchOverHandle == handle)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Got Linking SwitchOver ack."));
#endif
				UpdateLinkStage(LinkStageEnum::Linking);

				return;
			}
			break;
		case LinkStageEnum::Linking:
			if (SubState == (uint8_t)HostLinkingEnum::SwitchingToLinked
				&& SwitchOverHandle == handle)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Got Link SwitchOver ack."));
#endif
				UpdateLinkStage(LinkStageEnum::Linked);

				return;
			}
			break;
		default:
			break;
		}

#if defined(DEBUG_LOLA)
		this->Owner();
		Serial.println(F("Rejected SwitchOver ack."));
#endif
	}

protected:
	virtual const bool OnUnlinkedPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port, const uint8_t counter)
	{
		switch (port)
		{
		case Unlinked::PORT:
			if (LinkStage == LinkStageEnum::AwaitingLink)
			{
				switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
				{
				case Unlinked::SearchRequest::SUB_HEADER:
					if (payloadSize == Unlinked::SearchRequest::PAYLOAD_SIZE)
					{
						switch (SubState)
						{
						case HostAwaitingLinkEnum::Sleeping:
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Broadcast wake up!"));
#endif
							// Wake up!
							ResetStageStartTime();
							SubState = (uint8_t)HostAwaitingLinkEnum::NewSessionStart;
							PreLinkPacketSchedule = millis();
							SearchReplyPending = true;
							Task::enable();
							break;
						case HostAwaitingLinkEnum::BroadcastingSession:
							PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
							Task::enable();
							SearchReplyPending = true;
							break;
						default:
							break;
						}
					}
					break;
				case Unlinked::SessionRequest::SUB_HEADER:
					if (payloadSize == Unlinked::SessionRequest::PAYLOAD_SIZE)
					{
						switch (SubState)
						{
						case HostAwaitingLinkEnum::BroadcastingSession:
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Session start request received."));
#endif
							SearchReplyPending = false;
							Task::enable();
							break;
						default:
							break;
						}
					}
					break;
				case Unlinked::LinkingStartRequest::SUB_HEADER:
					if (SubState == (uint8_t)HostAwaitingLinkEnum::BroadcastingSession
						&& payloadSize == Unlinked::LinkingStartRequest::PAYLOAD_SIZE
						&& Session.SessionIdMatches(&payload[Unlinked::LinkingStartRequest::PAYLOAD_SESSION_ID_INDEX]))
					{
						// Slow operation (~8 ms).
						// The async alternative to inline decompressing of key would require a copy-buffer to hold the compressed key.
						Session.DecompressPartnerPublicKeyFrom(&payload[Unlinked::LinkingStartRequest::PAYLOAD_PUBLIC_KEY_INDEX]);
						SubState = (uint8_t)HostAwaitingLinkEnum::ValidatingSession;
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
				switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
				{
				case Linking::RemoteChallengeReplyRequest::SUB_HEADER:
					if (SubState == (uint8_t)HostLinkingEnum::AuthenticationRequest
						&& payloadSize == Linking::RemoteChallengeReplyRequest::PAYLOAD_SIZE
						&& Session.VerifyChallengeSignature(&payload[Linking::RemoteChallengeReplyRequest::PAYLOAD_SIGNED_INDEX]))
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Link Access Control granted."));
#endif

						Session.SetPartnerChallenge(&payload[Linking::RemoteChallengeReplyRequest::PAYLOAD_CHALLENGE_INDEX]);
						SubState = (uint8_t)HostLinkingEnum::AuthenticationReply;
						Task::enable();
					}
					break;
				case Linking::LinkStartRequest::SUB_HEADER:
					if (payloadSize == Linking::LinkStartRequest::PAYLOAD_SIZE)
					{
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Got LinkStartRequest."));
#endif
						switch (SubState)
						{
						case HostLinkingEnum::ClockSyncing:
							break;
						case HostLinkingEnum::AuthenticationReply:
							SubState = (uint8_t)HostLinkingEnum::ClockSyncing;
							ClockReplyPending = false;
							PreLinkPacketSchedule = millis();
							Task::enable();
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Started Clock sync"));
#endif
							break;
						case HostLinkingEnum::SwitchingToLinked:
							SubState = (uint8_t)HostLinkingEnum::ClockSyncing;
							ClockReplyPending = false;
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.println(F("Re-Started Clock sync"));
#endif
							break;
						default:
							return false;
						}

						InEstimate.Seconds = payload[Linking::LinkStartRequest::PAYLOAD_SECONDS_INDEX];
						InEstimate.Seconds += (uint_least16_t)payload[Linking::LinkStartRequest::PAYLOAD_SECONDS_INDEX + 1] << 8;
						InEstimate.Seconds += (uint32_t)payload[Linking::LinkStartRequest::PAYLOAD_SECONDS_INDEX + 2] << 16;
						InEstimate.Seconds += (uint32_t)payload[Linking::LinkStartRequest::PAYLOAD_SECONDS_INDEX + 3] << 24;
						InEstimate.SubSeconds = payload[Linking::LinkStartRequest::PAYLOAD_SUB_SECONDS_INDEX];
						InEstimate.SubSeconds += (uint_least16_t)payload[Linking::LinkStartRequest::PAYLOAD_SUB_SECONDS_INDEX + 1] << 8;
						InEstimate.SubSeconds += (uint32_t)payload[Linking::LinkStartRequest::PAYLOAD_SUB_SECONDS_INDEX + 2] << 16;
						InEstimate.SubSeconds += (uint32_t)payload[Linking::LinkStartRequest::PAYLOAD_SUB_SECONDS_INDEX + 3] << 24;

						if (!InTimestamp.Validate())
						{
							// Invalid estimate, sub-seconds should never match one second.
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.print(F("Clock sync Invalid estimate. SubSeconds="));
							Serial.print(InTimestamp.SubSeconds);
							Serial.println(F("us. "));
#endif
							return false;
						}

						if (InEstimateWithinTolerance(startTimestamp))
						{
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.print(F("Clock sync error: "));
							Serial.print(EstimateErrorReply.ErrorMicros());
							Serial.println(F("us Accepted!"));
#endif
							SubState = (uint8_t)HostLinkingEnum::SwitchingToLinked;
							ClockReplyPending = false;
						}
						else
						{
#if defined(DEBUG_LOLA)
							this->Owner();
							Serial.print(F("Link start clock error: "));
							Serial.print(EstimateErrorReply.ErrorMicros());
							Serial.println(F("us"));
#endif
							ClockReplyPending = true;
						}
						PreLinkPacketSchedule = millis();
						Task::enable();
					}
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
			SetHopperFixedChannel(Session.RandomSource.GetRandomShort());
			break;
		case LinkStageEnum::AwaitingLink:
			SubState = (uint8_t)HostAwaitingLinkEnum::Sleeping;
			break;
			//case LinkStageEnum::TransitionToLinking:
			//	break;
		case LinkStageEnum::Linking:
			SubState = (uint8_t)HostLinkingEnum::AuthenticationRequest;
			ClockReplyPending = false;
			break;
			//case LinkStageEnum::TransitionToLinked:
			//	break;
		case LinkStageEnum::Linked:
			break;
		default:
			break;
		}
	}

	virtual void OnAwaitingLink(const uint32_t stateElapsed) final
	{
		if (stateElapsed > HOST_SLEEP_TIMEOUT_MILLIS)
		{
#if defined(DEBUG_LOLA)
			this->Owner();
			Serial.println(F("OnAwaitingLink timed out. Going to sleep."));
#endif

			SubState = (uint8_t)HostAwaitingLinkEnum::Sleeping;
		}

		switch (SubState)
		{
		case HostAwaitingLinkEnum::NewSessionStart:
			Session.SetRandomSessionId();
			SyncClock.ShiftSeconds(Session.RandomSource.GetRandomLong());

			SubState = (uint8_t)HostAwaitingLinkEnum::BroadcastingSession;
			Task::enable();
			break;
		case HostAwaitingLinkEnum::BroadcastingSession:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				// Session broadcast packet.
				OutPacket.SetPort(Unlinked::PORT);
				if (SearchReplyPending)
				{
					OutPacket.Payload[Unlinked::SearchReply::SUB_HEADER_INDEX] = Unlinked::SearchReply::SUB_HEADER;

					if (SendPacket(this, LooseHandle, OutPacket.Data, Unlinked::SearchReply::PAYLOAD_SIZE))
					{
						SearchReplyPending = false;
						PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent Search Reply."));
#endif
					}
				}
				else
				{
					OutPacket.Payload[Unlinked::SessionBroadcast::SUB_HEADER_INDEX] = Unlinked::SessionBroadcast::SUB_HEADER;
					Session.CopySessionIdTo(&OutPacket.Payload[Unlinked::SessionBroadcast::PAYLOAD_SESSION_ID_INDEX]);
					Session.CompressPublicKeyTo(&OutPacket.Payload[Unlinked::SessionBroadcast::PAYLOAD_PUBLIC_KEY_INDEX]);

					if (SendPacket(this, LooseHandle, OutPacket.Data, Unlinked::SessionBroadcast::PAYLOAD_SIZE))
					{
						PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.println(F("Sent Broadcast Session."));
#endif
					}
				}
			}
			Task::enable();
			break;
		case HostAwaitingLinkEnum::ValidatingSession:
			if (Session.PublicKeyCollision())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Local and Partner public keys match, link is impossible."));
#endif
				SubState = (uint8_t)HostAwaitingLinkEnum::BroadcastingSession;
			}
			else if (Session.SessionIsCached())
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Session secrets are cached, let's start linking."));
#endif
				SubState = (uint8_t)HostAwaitingLinkEnum::SwitchingToLinking;
				PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::LinkingSwitchOver::PAYLOAD_SIZE);
			}
			else
			{
				Session.ResetPke();
				SubState = (uint8_t)HostAwaitingLinkEnum::ComputingSecretKey;
			}
			Task::enable();
			break;
		case HostAwaitingLinkEnum::ComputingSecretKey:
			if (Session.CalculatePke()) {
				// PKE calculation took a lot of time, let's for another start request, now with cached secrets.
				SubState = (uint8_t)HostAwaitingLinkEnum::BroadcastingSession;
				PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::SessionBroadcast::PAYLOAD_SIZE);;
			}
			Task::enable();
			break;
		case HostAwaitingLinkEnum::SwitchingToLinking:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Unlinked::PORT);
				OutPacket.Payload[Unlinked::LinkingSwitchOver::SUB_HEADER_INDEX] = Unlinked::LinkingSwitchOver::SUB_HEADER;
				Session.CopyLinkingTokenTo(&OutPacket.Payload[Unlinked::LinkingSwitchOver::PAYLOAD_SESSION_TOKEN_INDEX]);

				if (SendPacketWithAck(this, SwitchOverHandle, OutPacket.Data, Unlinked::LinkingSwitchOver::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Unlinked::LinkingSwitchOver::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Linking SwitchOver."));
#endif
				}
			}
			Task::enable();
			break;
		case HostAwaitingLinkEnum::Sleeping:
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
		case HostLinkingEnum::AuthenticationRequest:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::HostChallengeRequest::SUB_HEADER_INDEX] = Linking::HostChallengeRequest::SUB_HEADER;
				Session.CopyLocalChallengeTo(&OutPacket.Payload[Linking::HostChallengeRequest::PAYLOAD_CHALLENGE_INDEX]);

				if (SendPacket(this, LooseHandle, OutPacket.Data, Linking::HostChallengeRequest::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::HostChallengeRequest::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent HostChallengeRequest."));
#endif
				}
			}
			break;
		case HostLinkingEnum::AuthenticationReply:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::HostChallengeReply::SUB_HEADER_INDEX] = Linking::HostChallengeReply::SUB_HEADER;
				Session.SignPartnerChallengeTo(&OutPacket.Payload[Linking::HostChallengeReply::PAYLOAD_SIGNED_INDEX]);

				if (SendPacket(this, LooseHandle, OutPacket.Data, Linking::HostChallengeReply::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + PreLinkResendDelayMillis(Linking::HostChallengeReply::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent HostChallengeReply."));
#endif
				}
			}
			break;
		case HostLinkingEnum::ClockSyncing:
			if (ClockReplyPending && PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::LinkStartDeniedReply::SUB_HEADER_INDEX] = Linking::LinkStartDeniedReply::SUB_HEADER;
				OutPacket.Payload[Linking::LinkStartDeniedReply::PAYLOAD_SECONDS_INDEX + 0] = EstimateErrorReply.Seconds;
				OutPacket.Payload[Linking::LinkStartDeniedReply::PAYLOAD_SECONDS_INDEX + 1] = EstimateErrorReply.Seconds >> 8;
				OutPacket.Payload[Linking::LinkStartDeniedReply::PAYLOAD_SECONDS_INDEX + 2] = EstimateErrorReply.Seconds >> 16;
				OutPacket.Payload[Linking::LinkStartDeniedReply::PAYLOAD_SECONDS_INDEX + 3] = EstimateErrorReply.Seconds >> 24;

				OutPacket.Payload[Linking::LinkStartDeniedReply::PAYLOAD_SUB_SECONDS_INDEX + 0] = EstimateErrorReply.SubSeconds;
				OutPacket.Payload[Linking::LinkStartDeniedReply::PAYLOAD_SUB_SECONDS_INDEX + 1] = EstimateErrorReply.SubSeconds >> 8;
				OutPacket.Payload[Linking::LinkStartDeniedReply::PAYLOAD_SUB_SECONDS_INDEX + 2] = EstimateErrorReply.SubSeconds >> 16;
				OutPacket.Payload[Linking::LinkStartDeniedReply::PAYLOAD_SUB_SECONDS_INDEX + 3] = EstimateErrorReply.SubSeconds >> 24;

				if (SendPacket(this, LooseHandle, OutPacket.Data, Linking::LinkStartDeniedReply::PAYLOAD_SIZE))
				{
					// Only send a time reply once.
					ClockReplyPending = false;

#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Clock Error "));
#endif
				}
			}
			Task::enable();
			break;
		case HostLinkingEnum::SwitchingToLinked:
			if (PacketService.CanSendPacket() && (millis() > PreLinkPacketSchedule))
			{
				OutPacket.SetPort(Linking::PORT);
				OutPacket.Payload[Linking::LinkSwitchOver::SUB_HEADER_INDEX] = Linking::LinkSwitchOver::SUB_HEADER;
				OutPacket.Payload[Linking::LinkSwitchOver::PAYLOAD_SUB_SECONDS_INDEX + 0] = EstimateErrorReply.ErrorMicros();
				OutPacket.Payload[Linking::LinkSwitchOver::PAYLOAD_SUB_SECONDS_INDEX + 1] = EstimateErrorReply.ErrorMicros() >> 8;
				OutPacket.Payload[Linking::LinkSwitchOver::PAYLOAD_SUB_SECONDS_INDEX + 2] = EstimateErrorReply.ErrorMicros() >> 16;
				OutPacket.Payload[Linking::LinkSwitchOver::PAYLOAD_SUB_SECONDS_INDEX + 3] = EstimateErrorReply.ErrorMicros() >> 24;

				if (SendPacketWithAck(this, SwitchOverHandle, OutPacket.Data, Linking::LinkSwitchOver::PAYLOAD_SIZE))
				{
					PreLinkPacketSchedule = millis() + GetSendDuration(Linking::LinkSwitchOver::PAYLOAD_SIZE);
#if defined(DEBUG_LOLA)
					this->Owner();
					Serial.println(F("Sent Link SwitchOver."));
#endif
				}
			}
			else
			{
				Task::enable();
			}
			break;
		default:
			break;
		}
	}


#pragma endregion
private:
	const bool InEstimateWithinTolerance(const uint32_t receiveTimestamp)
	{
		const int32_t elapsedSinceReceived = micros() - receiveTimestamp;

		SyncClock.CalculateError(EstimateErrorReply, InEstimate, -elapsedSinceReceived);

		const int32_t errorMicros = EstimateErrorReply.ErrorMicros();

		if (errorMicros >= 0)
		{
			return errorMicros < LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
		}
		else
		{
			return (-errorMicros) < (int32_t)LoLaLinkDefinition::LINKING_CLOCK_TOLERANCE;
		}
	}
};
#endif