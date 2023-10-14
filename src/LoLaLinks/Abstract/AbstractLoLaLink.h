// AbstractLoLaLink.h
#ifndef _ABSTRACT_LOLA_LINK_
#define _ABSTRACT_LOLA_LINK_


#include "AbstractLoLaLinkPacket.h"
#include "..\..\Link\ReportTracker.h"
#include "..\..\Link\TimedStateTransition.h"
#include "..\..\Link\LinkClockTracker.h"

/// <summary>
/// 
/// </summary>
class AbstractLoLaLink : public AbstractLoLaLinkPacket
{
private:
	using BaseClass = AbstractLoLaLinkPacket;

	using Unlinked = LoLaLinkDefinition::Unlinked;
	using Linking = LoLaLinkDefinition::Linking;
	using Linked = LoLaLinkDefinition::Linked;

private:
	/// <summary>
	/// Slow value, let the main services hog the CPU during Link time.
	/// </summary>
	static constexpr uint8_t LINK_CHECK_PERIOD = 5;



protected:
	int32_t LinkSendDuration = 0;

private:
	ReportTracker QualityTracker{};

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

	virtual const uint8_t GetSyncClockQuality() { return 0; }

public:
	AbstractLoLaLink(Scheduler& scheduler,
		ILinkRegistry* linkRegistry,
		LoLaCryptoEncoderSession* encoder,
		ILoLaTransceiver* transceiver,
		ICycles* cycles,
		IEntropy* entropy,
		IDuplex* duplex,
		IChannelHop* hop)
		: BaseClass(scheduler, linkRegistry, encoder, transceiver, cycles, entropy, duplex, hop)
	{}

	virtual const bool Setup()
	{
		if (Registry->RegisterPacketListener(this, Linked::PORT))
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
		if (port == Linked::PORT
			&& payload[HeaderDefinition::HEADER_INDEX] == Linked::ReportUpdate::HEADER
			&& payloadSize == Linked::ReportUpdate::PAYLOAD_SIZE)
		{
			const uint_least16_t loopingDropCounter = payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX]
				| ((uint_least16_t)payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX + 1] << 8);

			// Keep track of partner's RSSI and packet drop, for output gain management.
			QualityTracker.OnReportReceived(millis(),
				loopingDropCounter,
				payload[Linked::ReportUpdate::PAYLOAD_RSSI_INDEX],
				payload[Linked::ReportUpdate::PAYLOAD_REQUEST_INDEX]);
		}
#if defined(DEBUG_LOLA)
		else
		{
			// This is the top-most class to parse incoming packets as a consumer.
			// So the base class call is not needed here.
			this->Skipped(F("Unknown"));
		}
#endif
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
	virtual void OnPacketReceivedOk(const uint8_t rssi, const uint8_t lostCount) final
	{
		QualityTracker.OnRxComplete(millis(), rssi, lostCount);
	}

protected:
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
			QualityTracker.ResetRssiQuality();
			break;
		case LinkStageEnum::Linking:
			break;
		case LinkStageEnum::Linked:
			SyncClock.GetTimestampMonotonic(LinkTimestamp);
			LinkStartSeconds = LinkTimestamp.Seconds;
			QualityTracker.Reset(millis());
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
			if (QualityTracker.GetLastValidReceivedAgeQuality() == 0)
			{
				// Zero quality means link has timed out.
				UpdateLinkStage(LinkStageEnum::AwaitingLink);
			}
			else if (CheckForReportUpdate())
			{
				// Report takes priority over clock, as it refers to counters and RSSI.
				CheckForClockSyncUpdate();
				Task::enable();
			}
			else if (CheckForClockSyncUpdate())
			{
				Task::enable();
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

#if defined(DEBUG_LOLA)
	virtual void OnEvent(const PacketEventEnum packetEvent)
	{
		this->Owner();
		switch (packetEvent)
		{
		case PacketEventEnum::ReceiveRejectedHeader:
			Serial.println(F("@Link Event: ReceiveRejected: Header/Counter"));
			break;
		case PacketEventEnum::ReceiveRejectedTransceiver:
			Serial.println(F("@Link Event: ReceiveRejected: Transceiver"));
			break;
		case PacketEventEnum::ReceiveRejectedDropped:
			Serial.println(F("@Link Event: ReceiveRejected: Dropped."));
			break;
		case PacketEventEnum::ReceiveRejectedInvalid:
			Serial.println(F("@Link Event: ReceiveRejected: Invalid."));
			break;
		case PacketEventEnum::ReceiveRejectedOutOfSlot:
			Serial.println(F("@Link Event: ReceiveRejected: OutOfSlot"));
			break;
		case PacketEventEnum::ReceiveRejectedMac:
			Serial.println(F("@Link Event: ReceiveRejectedMAC"));
			break;
		case PacketEventEnum::SendCollisionFailed:
			Serial.println(F("@Link Event: SendCollision: Failed"));
			break;
		default:
			Serial.println(F("@Link Event: Unknown."));
			break;
		}
	}
#endif

private:
	/// <summary>
	/// Sends update Report, if needed.
	/// </summary>
	/// <returns>True if a report update is due or pending to send.</returns>
	const bool CheckForReportUpdate()
	{
		const uint32_t timestamp = millis();

		QualityTracker.CheckUpdate(timestamp, GetSyncClockQuality());

		if (QualityTracker.IsReportSendRequested())
		{
			if (QualityTracker.IsTimeToSendReport(timestamp)
				&& CanRequestSend())
			{
				OutPacket.SetPort(Linked::PORT);

				const uint16_t rxLoopingDropCounter = QualityTracker.GetRxLoopingDropCount();

				OutPacket.Payload[Linked::ReportUpdate::HEADER_INDEX] = Linked::ReportUpdate::HEADER;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_REQUEST_INDEX] = QualityTracker.IsBackReportNeeded(timestamp);
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_RSSI_INDEX] = QualityTracker.GetRxRssiQuality();
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX] = rxLoopingDropCounter;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX + 1] = rxLoopingDropCounter >> 8;

				if (RequestSendPacket(Linked::ReportUpdate::PAYLOAD_SIZE))
				{
					QualityTracker.OnReportSent(timestamp);
				}
			}

			return true;
		}

		return false;
	}

#if defined(DEBUG_LOLA)
public:
	void LogQuality()
	{
		this->Owner();
		Serial.println(F("Quality Update"));
		Serial.print(F("\tRSSI <- "));
		Serial.println(QualityTracker.GetRxRssiQuality());
		Serial.print(F("\tRSSI -> "));
		Serial.println(QualityTracker.GetTxRssiQuality());

		Serial.print(F("\tRx: "));
		Serial.println(QualityTracker.GetRxDropQuality());
		Serial.print(F("\tRx Drop Counter: "));
		Serial.println(QualityTracker.GetRxLoopingDropCount());
		Serial.print(F("\tRx Drop Rate: "));
		Serial.println(QualityTracker.GetRxDropRate());
		Serial.print(F("\tTx: "));
		Serial.println(QualityTracker.GetTxDropQuality());
		Serial.print(F("\tTx Drop Rate: "));
		Serial.println(QualityTracker.GetTxDropRate());
		Serial.print(F("\tReceive Freshness: "));
		Serial.println(QualityTracker.GetLastValidReceivedAgeQuality());
		Serial.print(F("\tSync Clock: "));
		Serial.println(GetSyncClockQuality());

		Serial.println();
	}
#endif
};
#endif