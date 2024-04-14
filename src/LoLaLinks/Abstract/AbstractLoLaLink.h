// AbstractLoLaLink.h
#ifndef _ABSTRACT_LOLA_LINK_
#define _ABSTRACT_LOLA_LINK_


#include "AbstractLoLaLinkPacket.h"
#include "..\..\Link\LinkDefinitions.h"
#include "..\..\Link\ReportTracker.h"

using namespace LinkDefinitions;

/// <summary>
/// 
/// </summary>
class AbstractLoLaLink : public AbstractLoLaLinkPacket
{
private:
	using BaseClass = AbstractLoLaLinkPacket;

private:
	/// <summary>
	/// Slow value, let the main services hog the CPU during Link time.
	/// </summary>
	static constexpr uint8_t LINK_CHECK_PERIOD = 5;

protected:
	ReportTracker QualityTracker;

private:
	uint32_t LastUnlinkedSent = 0;

	uint32_t LinkingStartMillis = 0;
	Timestamp LinkStartTimestamp{};

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

	virtual const uint8_t GetClockSyncQuality() { return 0; }

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
		, QualityTracker(LoLaLinkDefinition::GetLinkTimeoutDuration(duplex->GetPeriod()) / ONE_MILLI_MICROS)
	{}

	virtual const bool Setup()
	{
		if (Registry->RegisterPacketListener(this, LoLaLinkDefinition::LINK_PORT))
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

	virtual void GetLinkStatus(LoLaLinkStatus& linkStatus) final
	{
		SyncClock.GetTimestampMonotonic(LinkTimestamp);
		linkStatus.DurationSeconds = LinkTimestamp.Seconds - LinkStartTimestamp.Seconds;
		linkStatus.RxDropCount = QualityTracker.GetRxDropCount();
		linkStatus.TxCount = SentCounter;
		linkStatus.RxCount = ReceivedCounter;
		linkStatus.RxDropRate = QualityTracker.GetRxDropRate();
		linkStatus.TxDropRate = QualityTracker.GetTxDropRate();
		linkStatus.Quality.RxRssi = QualityTracker.GetRxRssiQuality();
		linkStatus.Quality.TxRssi = QualityTracker.GetTxRssiQuality();
		linkStatus.Quality.RxDrop = QualityTracker.GetRxDropQuality();
		linkStatus.Quality.TxDrop = QualityTracker.GetTxDropQuality();
		linkStatus.Quality.ClockSync = GetClockSyncQuality();
		linkStatus.Quality.Age = QualityTracker.GetLastValidReceivedAgeQuality();
	}

	virtual const uint32_t GetLinkElapsed() final
	{
		if (HasLink())
		{
			SyncClock.GetTimestampMonotonic(LinkTimestamp);
			return LinkTimestamp.GetRollingMicros() - LinkStartTimestamp.GetRollingMicros();
		}
		else
		{
			return 0;
		}
	}

	virtual void OnSendComplete(const IPacketServiceListener::SendResultEnum result) final
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
		if (port == LoLaLinkDefinition::LINK_PORT
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
#if defined(DEBUG_LOLA_LINK)
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
			return false;
		}
	}

	virtual const bool Stop() final
	{
		if (LinkStage != LinkStageEnum::Disabled)
		{
			UpdateLinkStage(LinkStageEnum::Disabled);
			return Transceiver->Stop();
		}
		else
		{
			return false;
		}
	}

protected:
	virtual void OnPacketReceivedOk(const uint8_t rssi, const uint16_t lostCount) final
	{
		QualityTracker.OnRxComplete(millis(), rssi, lostCount);
	}

protected:
	const uint32_t GetLinkingElapsedMillis()
	{
		return millis() - LinkingStartMillis;
	}

	void ResetUnlinkedPacketThrottle()
	{
		LastUnlinkedSent -= GetPacketThrottlePeriod() * 2;
	}

	const bool UnlinkedPacketThrottle()
	{
		return micros() - LastUnlinkedSent >= GetPacketThrottlePeriod() * 2;
	}

protected:
	virtual void UpdateLinkStage(const LinkStageEnum linkStage)
	{
		switch (linkStage)
		{
		case LinkStageEnum::Disabled:
			SyncClock.Stop();
			Transceiver->Stop();
			break;
		case LinkStageEnum::Booting:
			SyncClock.ShiftSeconds(RandomSource.GetRandomLong());
			SyncClock.ShiftSubSeconds(RandomSource.GetRandomLong());
			break;
		case LinkStageEnum::AwaitingLink:
			RandomSource.RandomReseed();
			QualityTracker.ResetRssiQuality();
			break;
		case LinkStageEnum::Linking:
			LinkingStartMillis = millis();
			break;
		case LinkStageEnum::Linked:
			SyncClock.GetTimestampMonotonic(LinkStartTimestamp);
			QualityTracker.Reset(millis());
			break;
		default:
			break;
		}

		BaseClass::UpdateLinkStage(linkStage);

#if defined(DEBUG_LOLA_LINK)
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

	virtual void OnServiceRun() final
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
			if (GetLinkingElapsedMillis() > GetLinkingTimeoutDuration())
			{
#if defined(DEBUG_LOLA_LINK)
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

#if defined(DEBUG_LOLA_LINK)
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

		if (QualityTracker.CheckUpdate(timestamp))
		{
			if (QualityTracker.IsTimeToSendReport(timestamp)
				&& CanRequestSend())
			{
				const uint16_t rxLoopingDropCounter = QualityTracker.GetRxLoopingDropCount();
				OutPacket.SetPort(LoLaLinkDefinition::LINK_PORT);
				OutPacket.Payload[Linked::ReportUpdate::HEADER_INDEX] = Linked::ReportUpdate::HEADER;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_REQUEST_INDEX] = QualityTracker.IsBackReportNeeded(timestamp);
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_RSSI_INDEX] = QualityTracker.GetRxRssiQuality();
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX] = rxLoopingDropCounter;
				OutPacket.Payload[Linked::ReportUpdate::PAYLOAD_DROP_COUNTER_INDEX + 1] = rxLoopingDropCounter >> 8;

				if (RequestSendPacket(
					Linked::ReportUpdate::PAYLOAD_SIZE,
					GetReportPriority(QualityTracker.GetRxDropQuality(), QualityTracker.GetTxDropQuality())))
				{
					QualityTracker.OnReportSent(timestamp);
				}
			}

			return true;
		}

		return false;
	}

	static const RequestPriority GetReportPriority(const uint8_t rxDropQuality, const uint8_t txDropQuality)
	{
		uint8_t measure;
		if (rxDropQuality < txDropQuality)
		{
			measure = UINT8_MAX - rxDropQuality;
		}
		else
		{
			measure = UINT8_MAX - txDropQuality;
		}

		if (measure >= (UINT8_MAX / 4))
		{
			measure = UINT8_MAX;
		}
		else
		{
			measure *= 4;
		}

		return GetProgressPriority<RequestPriority::IRREGULAR, RequestPriority::RESERVED_FOR_LINK>(measure);
	}
};
#endif