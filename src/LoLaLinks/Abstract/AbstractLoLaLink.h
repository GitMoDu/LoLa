// AbstractLoLaLink.h
#ifndef _ABSTRACT_LOLA_LINK_
#define _ABSTRACT_LOLA_LINK_


#include "AbstractLoLaLinkPacket.h"
#include "..\..\Link\ReportTracker.h"
#include "..\..\Link\TimedStateTransition.h"


/// <summary>
/// 
/// </summary>
/// <typeparam name="MaxPayloadSize"></typeparam>
/// <typeparam name="MaxPacketReceiveListeners"></typeparam>
/// <typeparam name="MaxLinkListeners"></typeparam>
template<const uint8_t MaxPacketReceiveListeners = 10,
	const uint8_t MaxLinkListeners = 10>
class AbstractLoLaLink : public AbstractLoLaLinkPacket<LoLaLinkDefinition::LARGEST_PAYLOAD, MaxPacketReceiveListeners, MaxLinkListeners>
{
private:
	using BaseClass = AbstractLoLaLinkPacket<LoLaLinkDefinition::LARGEST_PAYLOAD, MaxPacketReceiveListeners, MaxLinkListeners>;


	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;


protected:
	using BaseClass::Transceiver;
	using BaseClass::OutPacket;
	using BaseClass::Duplex;
	using BaseClass::LinkStage;
	using BaseClass::LastValidReceivedCounter;
	using BaseClass::SendCounter;
	using BaseClass::SyncClock;
	using BaseClass::LastReceivedRssi;

	using BaseClass::RandomSource;
	using BaseClass::PacketService;

	using BaseClass::RegisterPacketReceiverInternal;
	using BaseClass::GetSendDuration;
	using BaseClass::CanRequestSend;
	using BaseClass::RequestSendPacket;
	using BaseClass::ResetLastValidReceived;

	using BaseClass::PostLinkState;
	using BaseClass::GetStageElapsedMillis;
	using BaseClass::GetElapsedSinceLastValidReceived;

private:
	/// <summary>
	/// Slow value, let the main services hog the CPU during Link time.
	/// </summary>
	static constexpr uint8_t LINK_CHECK_PERIOD = 5;


protected:
	Timestamp LinkTimestamp{};

private:
	ReportTracker ReportTracking;

	uint32_t LinkStartSeconds = 0;

protected:
	/// <summary>
	/// </summary>
	/// <param name="stateElapsed">Millis elapsed since link state start.</param>
	virtual void OnLinking(const uint32_t stateElapsed) {}

	/// <summary>
	/// </summary>
	/// <param name="stateElapsed">Millis elapsed since link state start.</param>
	virtual void OnAwaitingLink(const uint32_t stateElapsed) {}

	/// <summary>
	/// </summary>
	/// <returns>True if a clock sync update is due or pending to send.</returns>
	virtual const bool CheckForClockSyncUpdate() { return false; }

public:
	AbstractLoLaLink(Scheduler& scheduler,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		IEntropySource* entropySource,
		IClockSource* clockSource,
		ITimerSource* timerSource,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, encoder, transceiver, entropySource, clockSource, timerSource, duplex, hop)
		, ReportTracking()
	{}

	virtual const bool Setup()
	{
		if (BaseClass::Setup()
			&& RegisterPacketReceiverInternal(this, Linked::PORT))
		{
			return true;
		}

		return false;
	}

	virtual const uint32_t GetLinkDuration() final
	{
		SyncClock.GetTimestamp(LinkTimestamp);

		return LinkTimestamp.Seconds - LinkStartSeconds;
	}

	virtual void OnPacketReceived(const uint32_t startTimestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port)
	{
		if (port == Linked::PORT)
		{
			switch (payload[SubHeaderDefinition::SUB_HEADER_INDEX])
			{
			case Linked::ReportUpdate::SUB_HEADER:
				if (payloadSize == Linked::ReportUpdate::PAYLOAD_SIZE)
				{
					// Keep track of partner's RSSI, for output gain management.
					ReportTracking.OnReportReceived(millis(), payload[Linked::ReportUpdate::PAYLOAD_RSSI_INDEX]);

					// Update send counter, to prevent mismatches.
					if ((SendCounter - payload[Linked::ReportUpdate::PAYLOAD_RECEIVE_COUNTER_INDEX]) > LoLaLinkDefinition::ROLLING_COUNTER_TOLERANCE)
					{
						// Recover send counter to report provided.
#if defined(DEBUG_LOLA)
						this->Owner();
						Serial.print(F("Report recovered counter delta: "));
						Serial.println(SendCounter - payload[Linked::ReportUpdate::PAYLOAD_RECEIVE_COUNTER_INDEX] - LoLaLinkDefinition::ROLLING_COUNTER_TOLERANCE);
#endif
						SendCounter = payload[Linked::ReportUpdate::PAYLOAD_RECEIVE_COUNTER_INDEX] + LoLaLinkDefinition::ROLLING_COUNTER_TOLERANCE + 1;
					}

					// Check if partner is requesting a report back.
					if (payload[Linked::ReportUpdate::PAYLOAD_REQUEST_INDEX] > 0)
					{
						ReportTracking.RequestReportUpdate(false);
					}
				}
				break;
			default:
				// This is the top-most class to parse incoming packets as a consumer.
				// So the base class call is not needed here.
				break;
			}
		}
	}

	virtual const bool Start() final
	{
		if (LinkStage == LinkStageEnum::Disabled)
		{
			UpdateLinkStage(LinkStageEnum::Booting);

			return true;
		}
		else
		{
			Task::enableIfNot();

			return false;
		}
	}

	virtual const bool Stop() final
	{
		UpdateLinkStage(LinkStageEnum::Disabled);

		return Transceiver->Stop();
	}

protected:
#if defined(DEBUG_LOLA)
	virtual void Owner() {}
#endif

protected:
	/// <summary>
	/// Inform any Link class of packet loss detection,
	/// due to skipped counter.
	/// </summary>
	/// <param name="lostCount"></param>
	/// <returns></returns>
	virtual void OnReceiveLossDetected(const uint8_t lostCount)
	{
		OnEvent(PacketEventEnum::ReceiveLossDetected);
	}

	/// <summary>
	/// Calculates a re-send delay, with a random jitter if Duplex is not full.
	/// Since we are in a pre-link state, jitter is added for collision avoidance.
	/// </summary>
	/// <returns>Delay to apply to resend (in milliseconds).</returns>
	const uint32_t PreLinkResendDelayMillis(const uint8_t payloadSize)
	{
		const uint32_t duration = (LoLaLinkDefinition::REPLY_BASE_TIMEOUT_MICROS +
			GetSendDuration(payloadSize))
			/ 1000;

		if (Duplex->GetRange() == IDuplex::DUPLEX_FULL)
		{
			return duration;
		}
		else
		{

			return duration + RandomSource.GetRandomLong(duration);
		}
	}

	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			break;
		case LinkStageEnum::Booting:
			break;
		case LinkStageEnum::AwaitingLink:
			RandomSource.RandomReseed();
			PacketService.RefreshChannel();
			LastReceivedRssi = 0;
			break;
		case LinkStageEnum::Linking:
			break;
		case LinkStageEnum::Linked:
			SyncClock.GetTimestamp(LinkTimestamp);
			LinkStartSeconds = LinkTimestamp.Seconds;
			ResetLastValidReceived();
			ReportTracking.RequestReportUpdate(true);
			PacketService.RefreshChannel();
			break;
		default:
			break;
		}

#if defined(DEBUG_LOLA)
		this->Owner();

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			Serial.println(F("Stage = Disabled"));
			break;
		case LinkStageEnum::Booting:
			Serial.println(F("Stage = Booting"));
			break;
		case LinkStageEnum::AwaitingLink:
			Serial.println(F("Stage = AwaitingLink"));
			break;
		case LinkStageEnum::Linking:
			Serial.println(F("Stage = Linking"));
			break;
		case LinkStageEnum::Linked:
			Serial.println(F("Stage = Linked"));
			break;
		default:
			break;
		}
#endif
	}

	virtual void OnService() final
	{
		switch (LinkStage)
		{
		case LinkStageEnum::Disabled:
			SyncClock.Stop();
			Task::disable();
			break;
		case LinkStageEnum::Booting:
			SyncClock.Start();
			SyncClock.ShiftSeconds(RandomSource.GetRandomLong());
			if (Transceiver->Start())
			{
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else
			{
				UpdateLinkStage(LinkStageEnum::Disabled);
			}
			break;
		case LinkStageEnum::AwaitingLink:
			OnAwaitingLink(GetStageElapsedMillis());
			break;
		case LinkStageEnum::Linking:
			OnLinking(GetStageElapsedMillis());
			break;
		case LinkStageEnum::Linked:
			if (GetElapsedSinceLastValidReceived() > LoLaLinkDefinition::LINK_STAGE_TIMEOUT)
			{
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else if (CheckForReportUpdate())
			{
				// Report takes priority over clock, as it refers to counters and RSSI.
				Task::enableIfNot();
			}
			else if (CheckForClockSyncUpdate())
			{
				Task::enableIfNot();
			}
			else
			{
				Task::enableDelayed(LINK_CHECK_PERIOD);
			}
			break;
		default:
			break;
		}
	}

	virtual void OnEvent(const PacketEventEnum packetEvent)
	{
		BaseClass::OnEvent(packetEvent);


		switch (packetEvent)
		{
		case PacketEventEnum::ReceiveRejectedCounter:
			ReportTracking.RequestReportUpdate(true);
			break;
		default:
			break;
		}

#if defined(DEBUG_LOLA)
		switch (packetEvent)
		{
		case PacketEventEnum::Received:
			break;
		case PacketEventEnum::ReceiveRejectedCounter:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: Counter"));
			break;
		case PacketEventEnum::ReceiveRejectedDriver:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: Driver"));
			break;
		case PacketEventEnum::ReceiveRejectedDropped:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: Dropped."));
			break;
		case PacketEventEnum::ReceiveRejectedInvalid:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: Invalid."));
			break;
		case PacketEventEnum::ReceiveRejectedOutOfSlot:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: OutOfSlot"));
			break;
		case PacketEventEnum::ReceiveRejectedMac:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejectedMAC"));
			break;
		case PacketEventEnum::SendAckFailed:
			this->Owner();
			Serial.println(F("@Link Event: SendAck: Failed"));
			break;
		case PacketEventEnum::SendCollisionFailed:
			this->Owner();
			Serial.println(F("@Link Event: SendCollision: Failed"));
			break;
		case PacketEventEnum::ReceiveLossDetected:
			this->Owner();
			Serial.println(F("Link detected and recovered from lost packets."));
			break;
		case PacketEventEnum::Sent:
			break;
		case PacketEventEnum::SentAck:
			break;
		default:
			break;
		}
#endif

	}

private:
	/// <summary>
	/// Sends update Report, if needed.
	/// </summary>
	/// <returns>True if a report update is due or pending to send.</returns>
	const bool CheckForReportUpdate()
	{
		const uint32_t timestamp = millis();
		if (ReportTracking.IsSendRequested())
		{
			if (CanRequestSend())
			{
				OutPacket.SetPort(Linked::PORT);
				OutPacket.Payload[Linked::ReportUpdate::SUB_HEADER_INDEX] = Linked::ReportUpdate::SUB_HEADER;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_RSSI_INDEX] = LastReceivedRssi;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_RECEIVE_COUNTER_INDEX] = LastValidReceivedCounter;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_REQUEST_INDEX] = ReportTracking.IsBackReportNeeded(timestamp, GetElapsedSinceLastValidReceived());

				if (RequestSendPacket(Linked::ReportUpdate::PAYLOAD_SIZE))
				{
					ReportTracking.OnReportSent(timestamp);
				}
			}

			return true;
		}
		else if (ReportTracking.CheckReportNeeded(timestamp, GetElapsedSinceLastValidReceived()))
		{
			Task::enableIfNot();
			return true;
		}

		return false;
	}
};
#endif