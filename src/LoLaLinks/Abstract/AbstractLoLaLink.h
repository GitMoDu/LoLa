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
	using BaseClass::LinkStage;
	using BaseClass::LinkTimestamp;
	using BaseClass::LastValidReceivedCounter;
	using BaseClass::SendCounter;
	using BaseClass::SyncClock;
	using BaseClass::LastReceivedRssi;

	using BaseClass::RandomSource;
	using BaseClass::PacketService;

	using BaseClass::RegisterPacketListenerInternal;
	using BaseClass::GetSendDuration;
	using BaseClass::GetPacketThrottlePeriod;
	using BaseClass::CanRequestSend;
	using BaseClass::RequestSendPacket;
	using BaseClass::ResetLastValidReceived;

	using BaseClass::PostLinkState;
	using BaseClass::GetStageElapsedMillis;
	using BaseClass::GetElapsedSinceLastValidReceived;


protected:
	static const uint32_t ArrayToUInt32(const uint8_t* source)
	{
		uint32_t value = source[0];
		value += (uint32_t)source[1] << 8;
		value += (uint32_t)source[2] << 16;
		value += (uint32_t)source[3] << 24;

		return value;
	}

	static void UInt32ToArray(const uint32_t value, uint8_t* target)
	{
		target[0] = value;
		target[1] = value >> 8;
		target[2] = value >> 16;
		target[3] = value >> 24;
	}

	static void Int32ToArray(const int32_t value, uint8_t* target)
	{
		UInt32ToArray(value, target);
	}

	static const int32_t ArrayToInt32(const uint8_t* source)
	{
		return (int32_t)ArrayToUInt32(source);
	}

private:
	/// <summary>
	/// Slow value, let the main services hog the CPU during Link time.
	/// </summary>
	static constexpr uint8_t LINK_CHECK_PERIOD = 5;

protected:
	int32_t LinkSendDuration = 0;

private:
	ReportTracker ReportTracking;

	uint32_t LastUnlinkedSent = 0;

	uint32_t LinkStartSeconds = 0;

protected:
	/// <summary>
	/// </summary>
	virtual void OnServiceLinking() {}

	/// <summary>
	/// </summary>
	virtual void OnServiceAwaitingLink() {}

	/// <summary>
	/// </summary>
	/// <returns>True if a clock sync update is due or pending to send.</returns>
	virtual const bool CheckForClockSyncUpdate() { return false; }

public:
	AbstractLoLaLink(Scheduler& scheduler,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, encoder, transceiver, cycles, entropy, duplex, hop)
		, ReportTracking()
	{}

	virtual const bool Setup()
	{
		if (RegisterPacketListenerInternal(this, Linked::PORT))
		{
			return BaseClass::Setup();
		}
#if defined(DEBUG_LOLA)
		else
		{
			this->Owner();
			Serial.println(F("Register Link Port failed."));
		}
#endif

		return false;
	}

	virtual const uint32_t GetLinkDuration() final
	{
		SyncClock.GetTimestampMonotonic(LinkTimestamp);

		return LinkTimestamp.Seconds - LinkStartSeconds;
	}

	virtual void OnSendComplete(const IPacketServiceListener::SendResultEnum result)
	{
		switch (LinkStage)
		{
		case LinkStageEnum::AwaitingLink:
		case LinkStageEnum::Linking:
			if (result == IPacketServiceListener::SendResultEnum::Success)
			{
				LastUnlinkedSent = micros();
			}
			break;
		default:
			break;
		}
	}

	virtual void OnPacketReceived(const uint32_t timestamp, const uint8_t* payload, const uint8_t payloadSize, const uint8_t port)
	{
		if (port == Linked::PORT)
		{
			switch (payload[HeaderDefinition::HEADER_INDEX])
			{
			case Linked::ReportUpdate::HEADER:
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
#if defined(DEBUG_LOLA)
				else {
					this->Skipped(F("ReportUpdate"));
				}
#endif
				break;
			default:
				// This is the top-most class to parse incoming packets as a consumer.
				// So the base class call is not needed here.
#if defined(DEBUG_LOLA)
				this->Skipped(F("Unknown"));
#endif
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

	void ResetUnlinkedPacketThrottle()
	{
		LastUnlinkedSent -= GetPacketThrottlePeriod() * 2;
	}

	const bool UnlinkedPacketThrottle()
	{
		return micros() - LastUnlinkedSent >= GetPacketThrottlePeriod() * 2;
	}

	static const uint16_t GetPreLinkDuplexPeriod(IDuplex* duplex, ILoLaTransceiver* transceiver)
	{
		if (duplex->GetPeriod() == IDuplex::DUPLEX_FULL)
		{
			return (transceiver->GetTimeToAir(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
				+ transceiver->GetDurationInAir(LoLaPacketDefinition::MAX_PACKET_TOTAL_SIZE)
				+ LoLaLinkDefinition::FULL_DUPLEX_RESEND_WAIT_MICROS) * 2;
		}
		else
		{
			return duplex->GetPeriod() * 2;
		}
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		BaseClass::UpdateLinkStage(linkStage);

		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			SyncClock.Stop();
			Transceiver->Stop();
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
			SyncClock.GetTimestampMonotonic(LinkTimestamp);
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
			Task::disable();
			break;
		case LinkStageEnum::Booting:
			SyncClock.Start(0);
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
			OnServiceAwaitingLink();
			break;
		case LinkStageEnum::Linking:
			if (GetStageElapsedMillis() > LoLaLinkDefinition::LINKING_STAGE_TIMEOUT)
			{
#if defined(DEBUG_LOLA)
				this->Owner();
				Serial.println(F("Linking timed out!"));
#endif
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else
			{
				OnServiceLinking();
			}
			break;
		case LinkStageEnum::Linked:
			if (GetElapsedSinceLastValidReceived() > LoLaLinkDefinition::LINK_STAGE_TIMEOUT)
			{
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else if (CheckForReportUpdate())
			{
				// Report takes priority over clock, as it refers to counters and RSSI.
				CheckForClockSyncUpdate();
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
		case PacketEventEnum::ReceiveRejectedTransceiver:
			this->Owner();
			Serial.println(F("@Link Event: ReceiveRejected: Transceiver"));
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
		case PacketEventEnum::SendCollisionFailed:
			this->Owner();
			Serial.println(F("@Link Event: SendCollision: Failed"));
			break;
		case PacketEventEnum::ReceiveLossDetected:
			this->Owner();
			Serial.println(F("@Link Event: Detected lost packet(s)."));
			break;
		case PacketEventEnum::Sent:
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
				OutPacket.Payload[Linked::ReportUpdate::HEADER_INDEX] = Linked::ReportUpdate::HEADER;
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